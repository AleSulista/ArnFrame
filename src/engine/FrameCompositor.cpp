#include "FrameCompositor.h"

#include "ClipReaderPool.h"
#include "EffectProcessor.h"
#include "core/Clip.h"

#include <QFont>
#include <QFontMetrics>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
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

QImage imageForClip(const drift::Clip &clip, drift::TimeUs timelineUs, int width, int height)
{
    if (clip.path.isEmpty())
        return {};

    QImage image;
    if (clip.type == drift::ClipType::Image) {
        QImageReader reader(clip.path);
        image = reader.read();
        if (image.isNull())
            return {};
        image = image.convertToFormat(QImage::Format_RGBA8888)
                    .scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else if (clip.type == drift::ClipType::Video) {
        const drift::TimeUs sourceUs = clip.srcIn + (timelineUs - clip.timelineStart);
        image = ClipReaderPool::instance().readVideoFrame(clip.path, sourceUs, width, height);
    } else {
        return {};
    }

    if (image.isNull() || clip.effects.isEmpty())
        return image;

    return EffectProcessor::applyEffects(image, clip.effects);
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
    if (clip.opacity.isEmpty())
        return 1.0;
    const drift::TimeUs relative = timelineUs - clip.timelineStart;
    return qBound(0.0, clip.opacity.evaluateAt(relative), 1.0);
}

double transformValue(const drift::KeyframeTrack<double> &track, drift::TimeUs relative, double defaultValue)
{
    if (track.isEmpty())
        return defaultValue;
    return track.evaluateAt(relative);
}

void drawClipFrame(QPainter &painter, const QImage &frame, const drift::Clip &clip, drift::TimeUs timelineUs,
                   int canvasWidth, int canvasHeight)
{
    const drift::TimeUs relative = timelineUs - clip.timelineStart;
    const double posX = transformValue(clip.posX, relative, 0.5);
    const double posY = transformValue(clip.posY, relative, 0.5);
    const double scale = transformValue(clip.scale, relative, 1.0);
    const double rotation = transformValue(clip.rotation, relative, 0.0);

    painter.save();
    painter.setOpacity(opacityForClip(clip, timelineUs));
    painter.setCompositionMode(toQtComposition(clip.blendMode));
    painter.translate(posX * canvasWidth, posY * canvasHeight);
    painter.rotate(rotation);
    painter.scale(scale, scale);
    painter.drawImage(QPointF(-frame.width() / 2.0, -frame.height() / 2.0), frame);
    painter.restore();
}

void drawStyledText(QPainter &p, const drift::Clip &clip, const QString &text, int w, int h, double scale)
{
    const drift::TextStyle &s = clip.textStyle;
    QFont font(s.fontFamily);
    font.setPixelSize(qMax(8, static_cast<int>(s.pixelSize * scale)));
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
        tb.adjust(-s.boxPadding, -s.boxPadding, s.boxPadding, s.boxPadding);
        p.fillRect(tb, s.boxColor);
    }

    if (s.outlineWidth > 0.0) { // stroke via QPainterPath
        const QRect textBounds = fm.boundingRect(box, flag | Qt::AlignVCenter, text);
        QPainterPath path;
        path.addText(textBounds.left(), textBounds.top() + fm.ascent(), font, text);
        p.setPen(QPen(s.outlineColor, s.outlineWidth * scale));
        p.setBrush(s.color);
        p.drawPath(path);
    } else {
        p.setPen(s.color);
        p.drawText(box, flag | Qt::AlignVCenter, text);
    }
}

void drawTextClip(QPainter &painter, const drift::Clip &clip, drift::TimeUs timelineUs, int width, int height)
{
    const QString text = clip.textContent.isEmpty() ? clip.name : clip.textContent;
    if (text.isEmpty())
        return;

    const drift::TimeUs relative = timelineUs - clip.timelineStart;
    const double posX = transformValue(clip.posX, relative, 0.5);
    const double posY = transformValue(clip.posY, relative, 0.5);
    const double scale = transformValue(clip.scale, relative, 1.0);
    const double rotation = transformValue(clip.rotation, relative, 0.0);

    painter.save();
    painter.setOpacity(opacityForClip(clip, timelineUs));
    painter.setCompositionMode(toQtComposition(clip.blendMode));
    painter.translate(posX * width, posY * height);
    painter.rotate(rotation);
    drawStyledText(painter, clip, text, width, height, scale);
    painter.restore();
}

} // namespace

QImage FrameCompositor::compositeAt(drift::TimeUs timelineUs) const
{
    if (!m_project)
        return {};

    const int width = m_project->width();
    const int height = m_project->height();
    if (width <= 0 || height <= 0)
        return {};

    QSet<QString> videoPaths;
    QSet<QString> audioPaths;
    collectActivePaths(m_project, timelineUs, videoPaths, audioPaths);
    ClipReaderPool::instance().retainActivePaths(videoPaths, audioPaths);

    QImage canvas(width, height, QImage::Format_RGBA8888);
    canvas.fill(Qt::black);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Tracks are ordered top-to-bottom in the timeline (index 0 is the topmost
    // track), and the topmost track composites in front. Draw from the last
    // track up to index 0 so index 0 lands on top.
    const QList<drift::Track> &tracks = m_project->tracks();
    for (int ti = tracks.size() - 1; ti >= 0; --ti) {
        const drift::Track &track = tracks.at(ti);
        if (track.hidden || track.type == drift::TrackType::Audio)
            continue;

        for (const drift::Clip &clip : track.clips) {
            if (!clip.containsTime(timelineUs))
                continue;

            if (clip.type == drift::ClipType::Text) {
                drawTextClip(painter, clip, timelineUs, width, height);
                continue;
            }

            QImage frame;
            if (clip.type == drift::ClipType::Shape)
                frame = shapeImageForClip(clip, width, height);
            else
                frame = imageForClip(clip, timelineUs, width, height);
            if (frame.isNull())
                continue;

            drawClipFrame(painter, frame, clip, timelineUs, width, height);
        }
    }

    painter.end();
    return canvas;
}
