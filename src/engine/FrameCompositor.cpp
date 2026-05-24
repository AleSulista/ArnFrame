#include "FrameCompositor.h"

#include "ClipReaderPool.h"
#include "CompositorFrameHistory.h"
#include "EffectCatalog.h"
#include "EffectProcessor.h"
#include "MaskApplier.h"
#include "core/Clip.h"
#include "core/Time.h"
#include "core/Transition.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <cmath>
#include <QtMath>

namespace {

QPainter::CompositionMode toQtComposition(drift::BlendMode mode)
{
    switch (mode) {
    case drift::BlendMode::Multiply:
        return QPainter::CompositionMode_Multiply;
    case drift::BlendMode::Screen:
        return QPainter::CompositionMode_Screen;
    case drift::BlendMode::Overlay:
        return QPainter::CompositionMode_Overlay;
    case drift::BlendMode::Add:
        return QPainter::CompositionMode_Plus;
    case drift::BlendMode::Darken:
        return QPainter::CompositionMode_Darken;
    case drift::BlendMode::Lighten:
        return QPainter::CompositionMode_Lighten;
    case drift::BlendMode::Normal:
        break;
    }
    return QPainter::CompositionMode_SourceOver;
}

void collectActivePaths(const drift::Project *project, drift::TimeUs timelineUs, QSet<QString> &videoPaths,
                        QSet<QString> &audioPaths)
{
    if (!project)
        return;

    for (const drift::Track &track : project->tracks()) {
        if (track.hidden)
            continue;

        for (const drift::Clip &clip : track.clips) {
            if (!clip.containsTime(timelineUs) || clip.path.isEmpty())
                continue;
            if (clip.type == drift::ClipType::Shape)
                continue;

            if ((track.type == drift::TrackType::Video || track.type == drift::TrackType::Shape)
                && clip.type != drift::ClipType::Text)
                videoPaths.insert(clip.path);
            if (track.type == drift::TrackType::Audio
                || (track.type == drift::TrackType::Video && clip.type == drift::ClipType::Video)) {
                audioPaths.insert(clip.path);
            }
        }
    }
}

const drift::Effect *findTimeEchoEffect(const QList<drift::Effect> &effects)
{
    for (const drift::Effect &effect : effects) {
        if (effect.catalogId == QStringLiteral("time_echo"))
            return &effect;
    }
    return nullptr;
}

QList<drift::Effect> effectsExcludingTimeEcho(const QList<drift::Effect> &effects)
{
    QList<drift::Effect> filtered;
    filtered.reserve(effects.size());
    for (const drift::Effect &effect : effects) {
        if (effect.catalogId != QStringLiteral("time_echo"))
            filtered.append(effect);
    }
    return filtered;
}

QImage decodeClipMediaFrame(const drift::Clip &clip, drift::TimeUs timelineUs, int width, int height)
{
    if (clip.path.isEmpty())
        return {};

    if (clip.type == drift::ClipType::Image) {
        QImageReader reader(clip.path);
        QImage image = reader.read();
        if (image.isNull())
            return {};
        return image.convertToFormat(QImage::Format_RGBA8888)
            .scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (clip.type == drift::ClipType::Video) {
        const drift::TimeUs sourceUs = clip.timelineToSourceUs(timelineUs);
        return ClipReaderPool::instance().readVideoFrame(clip.path, sourceUs, width, height);
    }

    return {};
}

QImage shapeImageForClip(const drift::Clip &clip, int canvasWidth, int canvasHeight);

QImage imageForClip(const drift::Clip &clip, drift::TimeUs timelineUs, int width, int height, int projectFps,
                    int maxTimeEchoHistoryFrames)
{
    if (clip.type == drift::ClipType::Shape)
        return shapeImageForClip(clip, width, height);

    if (clip.path.isEmpty())
        return {};

    const drift::TimeUs clipTimeUs = timelineUs - clip.timelineStart;
    const drift::Effect *timeEcho = findTimeEchoEffect(clip.effects);
    const QList<drift::Effect> otherEffects = effectsExcludingTimeEcho(clip.effects);

    QImage image;
    if (timeEcho) {
        const EffectPresetEntry *def = effectDefForId(timeEcho->catalogId);
        if (!def)
            return {};

        const QMap<QString, QVariant> params = resolvedEffectParameters(*timeEcho, *def);
        int frameCount = qBound(1, params.value(QStringLiteral("frames"), 4).toInt(), 10);
        if (maxTimeEchoHistoryFrames >= 0)
            frameCount = qMin(frameCount, maxTimeEchoHistoryFrames);
        const double decay = qBound(0.0, params.value(QStringLiteral("decay"), 0.55).toDouble(), 1.0);
        const auto blendMode =
            CompositorFrameHistory::parseEchoBlendMode(params.value(QStringLiteral("blendMode")).toString());

        const drift::TimeUs frameStepUs = drift::frameDurationUs(projectFps);
        QList<QImage> samples;
        samples.reserve(frameCount + 1);

        const QImage current = decodeClipMediaFrame(clip, timelineUs, width, height);
        if (current.isNull())
            return {};
        samples.append(current);

        for (int i = 1; i <= frameCount; ++i) {
            const drift::TimeUs pastClipUs = clipTimeUs - static_cast<drift::TimeUs>(i) * frameStepUs;
            if (pastClipUs < 0)
                break;
            const drift::TimeUs pastTimelineUs = clip.timelineStart + pastClipUs;
            const QImage past = decodeClipMediaFrame(clip, pastTimelineUs, width, height);
            if (!past.isNull())
                samples.append(past);
        }

        image = CompositorFrameHistory::applyTimeEcho(samples, decay, blendMode);
    } else {
        image = decodeClipMediaFrame(clip, timelineUs, width, height);
    }

    if (image.isNull() || otherEffects.isEmpty()) {
        if (!image.isNull() && clip.mask.shape != drift::MaskShape::None)
            image = drift::applyMask(image, clip.mask, width, height);
        return image;
    }

    image = EffectProcessor::applyEffects(image, otherEffects, clipTimeUs);
    if (clip.mask.shape != drift::MaskShape::None)
        image = drift::applyMask(image, clip.mask, width, height);
    return image;
}

QImage shapeImageForClip(const drift::Clip &clip, int canvasWidth, int canvasHeight)
{
    if (clip.type != drift::ClipType::Shape)
        return {};

    const int base = qMin(canvasWidth, canvasHeight);
    int w = 0;
    int h = 0;
    switch (clip.shapeStyle.kind) {
    case drift::ShapeKind::Rectangle:
        w = qMax(16, static_cast<int>(canvasWidth * 0.30));
        h = qMax(16, static_cast<int>(canvasHeight * 0.20));
        break;
    case drift::ShapeKind::Square:
        w = h = qMax(16, static_cast<int>(base * 0.25));
        break;
    case drift::ShapeKind::Triangle:
    case drift::ShapeKind::Pentagon:
    case drift::ShapeKind::Hexagon:
        w = h = qMax(16, static_cast<int>(base * 0.30));
        break;
    }

    QImage image(w, h, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);

    const drift::ShapeStyle &style = clip.shapeStyle;
    QPen pen(style.stroke, style.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(style.fill);

    const QPointF center(w / 2.0, h / 2.0);
    if (clip.shapeStyle.kind == drift::ShapeKind::Rectangle) {
        const double inset = style.strokeWidth / 2.0;
        p.drawRect(QRectF(inset, inset, w - style.strokeWidth, h - style.strokeWidth));
    } else if (clip.shapeStyle.kind == drift::ShapeKind::Square) {
        const double side = qMin(w, h) - style.strokeWidth;
        p.drawRect(QRectF((w - side) / 2.0, (h - side) / 2.0, side, side));
    } else {
        int sides = 3;
        switch (clip.shapeStyle.kind) {
        case drift::ShapeKind::Triangle:
            sides = 3;
            break;
        case drift::ShapeKind::Pentagon:
            sides = 5;
            break;
        case drift::ShapeKind::Hexagon:
            sides = 6;
            break;
        default:
            break;
        }
        const double radius = qMin(w, h) / 2.0 - style.strokeWidth;
        QPainterPath path;
        for (int i = 0; i < sides; ++i) {
            const double angle = -M_PI_2 + i * 2.0 * M_PI / sides;
            const QPointF pt(center.x() + radius * qCos(angle), center.y() + radius * qSin(angle));
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        path.closeSubpath();
        p.drawPath(path);
    }

    p.end();
    return image;
}

double opacityForClip(const drift::Clip &clip, drift::TimeUs timelineUs)
{
    double value = 1.0;
    if (!clip.opacity.isEmpty()) {
        const drift::TimeUs relative = timelineUs - clip.timelineStart;
        value = qBound(0.0, clip.opacity.evaluateAt(relative), 1.0);
    }
    // Edge-relative fades ride on top of any opacity keyframes.
    return value * clip.fadeMultiplier(timelineUs);
}

double transformValue(const drift::KeyframeTrack<double> &track, drift::TimeUs relative, double defaultValue)
{
    if (track.isEmpty())
        return defaultValue;
    return track.evaluateAt(relative);
}

// Layout is stored in project pixels. Preview/export canvases may be scaled
// via renderScale — always map project → canvas here so WYSIWYG handles match.
void layoutRectForClip(const drift::Clip &clip, drift::TimeUs timelineUs, int projectWidth, int projectHeight,
                       double renderScale, double extraScale, double *xOut, double *yOut, double *wOut, double *hOut,
                       double *rotationOut = nullptr)
{
    const drift::TimeUs relative = timelineUs - clip.timelineStart;
    const double scale = renderScale * extraScale;
    *xOut = transformValue(clip.transformX, relative, 0.0) * renderScale;
    *yOut = transformValue(clip.transformY, relative, 0.0) * renderScale;
    *wOut = transformValue(clip.transformW, relative, static_cast<double>(projectWidth)) * scale;
    *hOut = transformValue(clip.transformH, relative, static_cast<double>(projectHeight)) * scale;
    if (rotationOut)
        *rotationOut = transformValue(clip.rotation, relative, 0.0);
}

void drawClipFrame(QPainter &painter, const QImage &frame, const drift::Clip &clip, drift::TimeUs timelineUs,
                   int projectWidth, int projectHeight, double renderScale, double opacityMultiplier = 1.0,
                   const QRectF *canvasClipRect = nullptr, QPointF extraOffset = QPointF(), double extraScale = 1.0)
{
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double rotation = 0.0;
    layoutRectForClip(clip, timelineUs, projectWidth, projectHeight, renderScale, extraScale, &x, &y, &w, &h,
                      &rotation);
    if (w <= 0.5 || h <= 0.5 || frame.isNull())
        return;

    const QImage drawn = (frame.width() == qRound(w) && frame.height() == qRound(h))
                             ? frame
                             : frame.scaled(qMax(1, qRound(w)), qMax(1, qRound(h)), Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation);

    painter.save();
    if (canvasClipRect)
        painter.setClipRect(*canvasClipRect, Qt::IntersectClip);
    painter.setOpacity(opacityForClip(clip, timelineUs) * qBound(0.0, opacityMultiplier, 1.0));
    painter.setCompositionMode(toQtComposition(clip.blendMode));
    painter.translate(x + w * 0.5 + extraOffset.x(), y + h * 0.5 + extraOffset.y());
    painter.rotate(rotation);
    painter.drawImage(QPointF(-w * 0.5, -h * 0.5), drawn);
    painter.restore();
}

QRectF incomingWipeClipRect(drift::TransitionKind kind, double progress, int width, int height)
{
    const double p = qBound(0.0, progress, 1.0);
    switch (kind) {
    case drift::TransitionKind::WipeLeft:
        return QRectF(0, 0, width * p, height);
    case drift::TransitionKind::WipeRight: {
        const double x = width * (1.0 - p);
        return QRectF(x, 0, width - x, height);
    }
    case drift::TransitionKind::WipeUp: {
        const double y = height * (1.0 - p);
        return QRectF(0, y, width, height - y);
    }
    case drift::TransitionKind::WipeDown:
        return QRectF(0, 0, width, height * p);
    default:
        return QRectF(0, 0, width, height);
    }
}

bool isWipeKind(drift::TransitionKind kind)
{
    switch (kind) {
    case drift::TransitionKind::WipeLeft:
    case drift::TransitionKind::WipeRight:
    case drift::TransitionKind::WipeUp:
    case drift::TransitionKind::WipeDown:
        return true;
    default:
        return false;
    }
}

void drawTransitionFrame(QPainter &painter, const drift::Transition &transition, const drift::Clip &fromClip,
                         const drift::Clip &toClip, drift::TimeUs timelineUs, drift::TimeUs windowStart,
                         drift::TimeUs windowEnd, int projectWidth, int projectHeight, double renderScale,
                         int canvasWidth, int canvasHeight, int projectFps, int maxTimeEchoHistoryFrames)
{
    const double p = drift::transitionProgress(timelineUs, windowStart, windowEnd);
    const drift::TransitionBlendOpacities blend = drift::transitionBlendOpacities(transition.kind, p);

    auto decodeLayout = [&](const drift::Clip &clip) {
        double x = 0.0;
        double y = 0.0;
        double w = 0.0;
        double h = 0.0;
        layoutRectForClip(clip, timelineUs, projectWidth, projectHeight, renderScale, 1.0, &x, &y, &w, &h);
        return imageForClip(clip, timelineUs, qMax(1, qRound(w)), qMax(1, qRound(h)), projectFps,
                            maxTimeEchoHistoryFrames);
    };

    const QImage frameA = decodeLayout(fromClip);
    const QImage frameB = decodeLayout(toClip);
    if (frameA.isNull() && frameB.isNull())
        return;

    if (transition.kind == drift::TransitionKind::PushLeft) {
        const QPointF outOffset(-canvasWidth * p, 0);
        const QPointF inOffset(canvasWidth * (1.0 - p), 0);
        if (!frameA.isNull())
            drawClipFrame(painter, frameA, fromClip, timelineUs, projectWidth, projectHeight, renderScale,
                          blend.outgoing, nullptr, outOffset);
        if (!frameB.isNull())
            drawClipFrame(painter, frameB, toClip, timelineUs, projectWidth, projectHeight, renderScale,
                          blend.incoming, nullptr, inOffset);
        return;
    }

    if (transition.kind == drift::TransitionKind::ZoomIn) {
        const double inScale = 0.82 + 0.18 * p;
        if (!frameA.isNull())
            drawClipFrame(painter, frameA, fromClip, timelineUs, projectWidth, projectHeight, renderScale,
                          blend.outgoing);
        if (!frameB.isNull())
            drawClipFrame(painter, frameB, toClip, timelineUs, projectWidth, projectHeight, renderScale,
                          blend.incoming, nullptr, QPointF(), inScale);
        return;
    }

    if (isWipeKind(transition.kind)) {
        if (!frameA.isNull())
            drawClipFrame(painter, frameA, fromClip, timelineUs, projectWidth, projectHeight, renderScale,
                          blend.outgoing);
        if (!frameB.isNull()) {
            const QRectF wipeClip = incomingWipeClipRect(transition.kind, p, canvasWidth, canvasHeight);
            drawClipFrame(painter, frameB, toClip, timelineUs, projectWidth, projectHeight, renderScale,
                          blend.incoming, &wipeClip);
        }
        return;
    }

    if (!frameA.isNull())
        drawClipFrame(painter, frameA, fromClip, timelineUs, projectWidth, projectHeight, renderScale,
                      blend.outgoing);
    if (!frameB.isNull())
        drawClipFrame(painter, frameB, toClip, timelineUs, projectWidth, projectHeight, renderScale,
                      blend.incoming);

    if (blend.blackOverlay > 0.0) {
        painter.save();
        painter.setOpacity(blend.blackOverlay);
        painter.fillRect(0, 0, canvasWidth, canvasHeight, Qt::black);
        painter.restore();
    }
    if (blend.whiteOverlay > 0.0) {
        painter.save();
        painter.setOpacity(blend.whiteOverlay);
        painter.fillRect(0, 0, canvasWidth, canvasHeight, Qt::white);
        painter.restore();
    }
}

void drawStyledText(QPainter &p, const drift::Clip &clip, const QString &text, int w, int h, double scale,
                    double renderScale)
{
    const drift::TextStyle &s = clip.textStyle;
    QFont font(s.fontFamily);
    font.setPixelSize(qMax(8, static_cast<int>(s.pixelSize * scale * renderScale)));
    font.setBold(s.bold);
    font.setItalic(s.italic);
    p.setFont(font);

    const int flag = s.align == drift::TextAlign::Left    ? Qt::AlignLeft
                     : s.align == drift::TextAlign::Right ? Qt::AlignRight
                                                           : Qt::AlignHCenter;
    const QRect box(-w / 2, -h / 2, w, h);
    const QFontMetrics fm(font);

    if (s.boxEnabled) {
        QRect tb = fm.boundingRect(box, flag | Qt::AlignVCenter, text);
        const int padding = qMax(0, static_cast<int>(s.boxPadding * renderScale));
        tb.adjust(-padding, -padding, padding, padding);
        p.fillRect(tb, s.boxColor);
    }

    if (s.outlineWidth > 0.0) { // stroke via QPainterPath
        const QRect textBounds = fm.boundingRect(box, flag | Qt::AlignVCenter, text);
        QPainterPath path;
        path.addText(textBounds.left(), textBounds.top() + fm.ascent(), font, text);
        p.setPen(QPen(s.outlineColor, s.outlineWidth * scale * renderScale));
        p.setBrush(s.color);
        p.drawPath(path);
    } else {
        p.setPen(s.color);
        p.drawText(box, flag | Qt::AlignVCenter, text);
    }
}

void drawTextClip(QPainter &painter, const drift::Clip &clip, drift::TimeUs timelineUs, int projectWidth,
                  int projectHeight, double renderScale)
{
    const QString text = clip.textContent.isEmpty() ? clip.name : clip.textContent;
    if (text.isEmpty())
        return;

    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double rotation = 0.0;
    layoutRectForClip(clip, timelineUs, projectWidth, projectHeight, renderScale, 1.0, &x, &y, &w, &h, &rotation);
    if (w <= 0.5 || h <= 0.5)
        return;

    painter.save();
    painter.setOpacity(opacityForClip(clip, timelineUs));
    painter.setCompositionMode(toQtComposition(clip.blendMode));
    painter.translate(x + w * 0.5, y + h * 0.5);
    painter.rotate(rotation);
    drawStyledText(painter, clip, text, qMax(1, qRound(w)), qMax(1, qRound(h)), 1.0, renderScale);
    painter.restore();
}

// Fast separable box blur (two passes ≈ gaussian) over an RGBA8888 image.
QImage boxBlurRgba(const QImage &src, int radius)
{
    if (src.isNull() || radius <= 0)
        return src;

    QImage image = src.convertToFormat(QImage::Format_RGBA8888);
    const int w = image.width();
    const int h = image.height();
    const int r = qMin(radius, qMax(1, qMin(w, h) / 2));
    const int window = r * 2 + 1;

    auto blurPass = [&](const QImage &in, bool horizontal) {
        QImage out(in.size(), QImage::Format_RGBA8888);
        const int lines = horizontal ? h : w;
        const int span = horizontal ? w : h;
        for (int line = 0; line < lines; ++line) {
            int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            auto sample = [&](int i) -> const uchar * {
                const int cx = horizontal ? i : line;
                const int cy = horizontal ? line : i;
                return in.constScanLine(cy) + cx * 4;
            };
            auto put = [&](int i, int rr, int gg, int bb, int aa) {
                const int cx = horizontal ? i : line;
                const int cy = horizontal ? line : i;
                uchar *px = out.scanLine(cy) + cx * 4;
                px[0] = static_cast<uchar>(rr / window);
                px[1] = static_cast<uchar>(gg / window);
                px[2] = static_cast<uchar>(bb / window);
                px[3] = static_cast<uchar>(aa / window);
            };
            for (int k = -r; k <= r; ++k) {
                const uchar *px = sample(qBound(0, k, span - 1));
                sumR += px[0];
                sumG += px[1];
                sumB += px[2];
                sumA += px[3];
            }
            for (int i = 0; i < span; ++i) {
                put(i, sumR, sumG, sumB, sumA);
                const uchar *addPx = sample(qBound(0, i + r + 1, span - 1));
                const uchar *subPx = sample(qBound(0, i - r, span - 1));
                sumR += addPx[0] - subPx[0];
                sumG += addPx[1] - subPx[1];
                sumB += addPx[2] - subPx[2];
                sumA += addPx[3] - subPx[3];
            }
        }
        return out;
    };

    image = blurPass(image, true);
    image = blurPass(image, false);
    return image;
}

// The topmost active video/image frame at this time, used to derive a blur fill.
QImage topmostVisualFrame(const drift::Project &project, drift::TimeUs timelineUs, int width, int height)
{
    const QList<drift::Track> &tracks = project.tracks();
    for (const drift::Track &track : tracks) {
        if (track.hidden || track.type == drift::TrackType::Audio)
            continue;
        for (const drift::Clip &clip : track.clips) {
            if (!clip.containsTime(timelineUs))
                continue;
            if (clip.type != drift::ClipType::Video && clip.type != drift::ClipType::Image)
                continue;
            QImage frame = imageForClip(clip, timelineUs, width, height, project.fps(), -1);
            if (!frame.isNull())
                return frame;
        }
    }
    return {};
}

// Fills the canvas behind all clips from the project's background setting.
void fillBackground(QPainter &painter, const drift::Project &project, drift::TimeUs timelineUs, int width,
                    int height)
{
    const drift::Background &bg = project.background();

    if (bg.kind == drift::BackgroundKind::Blur) {
        const QImage frame = topmostVisualFrame(project, timelineUs, width, height);
        if (!frame.isNull()) {
            const QImage cover = frame.convertToFormat(QImage::Format_RGBA8888)
                                     .scaled(width, height, Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);
            const QImage blurred = boxBlurRgba(cover, qMax(1, static_cast<int>(bg.blurStrength)));
            const int dx = (blurred.width() - width) / 2;
            const int dy = (blurred.height() - height) / 2;
            painter.fillRect(0, 0, width, height, Qt::black);
            painter.drawImage(QPoint(0, 0), blurred, QRect(dx, dy, width, height));
            return;
        }
        painter.fillRect(0, 0, width, height, Qt::black);
        return;
    }

    painter.fillRect(0, 0, width, height, bg.color.isValid() ? bg.color : QColor(Qt::black));
}

} // namespace

QImage FrameCompositor::compositeAt(drift::TimeUs timelineUs) const
{
    return compositeAt(timelineUs, RenderOptions{});
}

QImage FrameCompositor::compositeAt(drift::TimeUs timelineUs, const RenderOptions &options) const
{
    if (!m_project)
        return {};

    const int projectWidth = m_project->width();
    const int projectHeight = m_project->height();
    const double renderScale = qBound(0.1, options.previewScale, 1.0);
    const int width = qMax(1, static_cast<int>(std::lround(projectWidth * renderScale)));
    const int height = qMax(1, static_cast<int>(std::lround(projectHeight * renderScale)));
    if (width <= 0 || height <= 0)
        return {};

    QSet<QString> videoPaths;
    QSet<QString> audioPaths;
    collectActivePaths(m_project, timelineUs, videoPaths, audioPaths);
    ClipReaderPool::instance().retainActivePaths(videoPaths, audioPaths);

    QImage canvas(width, height, QImage::Format_RGBA8888);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    fillBackground(painter, *m_project, timelineUs, width, height);

    // Tracks are ordered top-to-bottom in the timeline (index 0 is the topmost
    // track), and the topmost track composites in front. Draw from the last
    // track up to index 0 so index 0 lands on top.
    const QList<drift::Track> &tracks = m_project->tracks();
    for (int ti = tracks.size() - 1; ti >= 0; --ti) {
        const drift::Track &track = tracks.at(ti);
        if (track.hidden || track.type == drift::TrackType::Audio)
            continue;

        QSet<QString> transitionClipIds;
        drift::TimeUs transitionStart = 0;
        drift::TimeUs transitionEnd = 0;
        const drift::Transition *activeTransition =
            drift::activeTransitionAt(track, timelineUs, transitionStart, transitionEnd);
        if (activeTransition) {
            const drift::Clip *fromClip = drift::clipById(track, activeTransition->fromClipId);
            const drift::Clip *toClip = drift::clipById(track, activeTransition->toClipId);
            if (fromClip && toClip) {
                drawTransitionFrame(painter, *activeTransition, *fromClip, *toClip, timelineUs, transitionStart,
                                    transitionEnd, projectWidth, projectHeight, renderScale, width, height,
                                    m_project->fps(), options.maxTimeEchoHistoryFrames);
                transitionClipIds.insert(fromClip->id);
                transitionClipIds.insert(toClip->id);
            }
        }

        for (const drift::Clip &clip : track.clips) {
            if (transitionClipIds.contains(clip.id))
                continue;

            if (!clip.containsTime(timelineUs))
                continue;

            if (clip.type == drift::ClipType::Text) {
                drawTextClip(painter, clip, timelineUs, projectWidth, projectHeight, renderScale);
                continue;
            }

            double layoutX = 0.0;
            double layoutY = 0.0;
            double layoutWd = 0.0;
            double layoutHd = 0.0;
            layoutRectForClip(clip, timelineUs, projectWidth, projectHeight, renderScale, 1.0, &layoutX, &layoutY,
                              &layoutWd, &layoutHd);
            const int layoutW = qMax(1, qRound(layoutWd));
            const int layoutH = qMax(1, qRound(layoutHd));

            QImage frame;
            if (clip.type == drift::ClipType::Shape) {
                frame = shapeImageForClip(clip, layoutW, layoutH);
                if (!frame.isNull() && clip.mask.shape != drift::MaskShape::None)
                    frame = drift::applyMask(frame, clip.mask, layoutW, layoutH);
            } else {
                frame = imageForClip(clip, timelineUs, layoutW, layoutH, m_project->fps(),
                                     options.maxTimeEchoHistoryFrames);
            }
            if (frame.isNull())
                continue;

            drawClipFrame(painter, frame, clip, timelineUs, projectWidth, projectHeight, renderScale);
        }
    }

    painter.end();
    return canvas;
}
