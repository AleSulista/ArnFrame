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

            if (track.type == drift::TrackType::Video && clip.type != drift::ClipType::Text)
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

    for (const drift::Track &track : m_project->tracks()) {
        if (track.hidden || track.type != drift::TrackType::Video)
            continue;

        for (const drift::Clip &clip : track.clips) {
            if (!clip.containsTime(timelineUs))
                continue;

            const QImage frame = imageForClip(clip, timelineUs, width, height);
            if (frame.isNull())
                continue;

            drawClipFrame(painter, frame, clip, timelineUs, width, height);
        }
    }

    for (const drift::Track &track : m_project->tracks()) {
        if (track.hidden || track.type != drift::TrackType::Text)
            continue;

        for (const drift::Clip &clip : track.clips) {
            if (!clip.containsTime(timelineUs))
                continue;

            const QString text = clip.textContent.isEmpty() ? clip.name : clip.textContent;
            if (text.isEmpty())
                continue;

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
    }

    painter.end();
    return canvas;
}
