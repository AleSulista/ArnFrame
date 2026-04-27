#include "FrameCompositor.h"

#include "ClipReaderPool.h"
#include "EffectProcessor.h"
#include "core/Clip.h"

#include <QImageReader>
#include <QPainter>
#include <QFont>
#include <QSet>
#include <QtMath>

namespace {

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
    painter.translate(posX * canvasWidth, posY * canvasHeight);
    painter.rotate(rotation);
    painter.scale(scale, scale);
    painter.drawImage(QPointF(-frame.width() / 2.0, -frame.height() / 2.0), frame);
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
            painter.translate(posX * width, posY * height);
            painter.rotate(rotation);
            painter.scale(scale, scale);

            QFont font = painter.font();
            font.setPixelSize(qMax(24, static_cast<int>(height / 12 * scale)));
            font.setBold(true);
            painter.setFont(font);
            painter.setPen(Qt::white);

            const QRect textRect(-width / 2, -height / 2, width, height);
            painter.drawText(textRect, Qt::AlignCenter, text);
            painter.restore();
        }
    }

    painter.end();
    return canvas;
}
