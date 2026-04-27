#include "ClipReaderWorker.h"

ClipReaderWorker::ClipReaderWorker(QObject *parent)
    : QObject(parent)
{
}

void ClipReaderWorker::openPath(const QString &path)
{
    QMutexLocker lock(&m_mutex);
    m_cachedSourceUs = -1;
    m_cachedFrame = {};
    m_reader.open(path);
}

void ClipReaderWorker::closePath()
{
    QMutexLocker lock(&m_mutex);
    m_cachedSourceUs = -1;
    m_cachedFrame = {};
    m_reader.close();
}

QImage ClipReaderWorker::decodeVideo(drift::TimeUs sourceUs, int targetWidth, int targetHeight)
{
    QMutexLocker lock(&m_mutex);
    if (m_cachedSourceUs == sourceUs && m_cachedWidth == targetWidth && m_cachedHeight == targetHeight
        && !m_cachedFrame.isNull()) {
        return m_cachedFrame;
    }

    QImage frame;
    if (!m_reader.readVideoFrameAt(sourceUs, frame, targetWidth, targetHeight))
        return {};

    m_cachedSourceUs = sourceUs;
    m_cachedWidth = targetWidth;
    m_cachedHeight = targetHeight;
    m_cachedFrame = frame;
    return frame;
}

int ClipReaderWorker::decodeAudio(drift::TimeUs sourceStartUs, int sampleCount, int outputSampleRate,
                                  float *interleavedStereoOut)
{
    QMutexLocker lock(&m_mutex);
    return m_reader.readAudioInterleaved(sourceStartUs, sampleCount, outputSampleRate, interleavedStereoOut);
}

void ClipReaderWorker::prefetchVideo(drift::TimeUs sourceUs, int targetWidth, int targetHeight)
{
    QMutexLocker lock(&m_mutex);
    if (m_cachedSourceUs == sourceUs && m_cachedWidth == targetWidth && m_cachedHeight == targetHeight
        && !m_cachedFrame.isNull()) {
        return;
    }

    QImage frame;
    if (!m_reader.readVideoFrameAt(sourceUs, frame, targetWidth, targetHeight))
        return;

    m_cachedSourceUs = sourceUs;
    m_cachedWidth = targetWidth;
    m_cachedHeight = targetHeight;
    m_cachedFrame = frame;
}
