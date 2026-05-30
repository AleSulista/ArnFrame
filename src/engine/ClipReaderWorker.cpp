#include "ClipReaderWorker.h"

ClipReaderWorker::ClipReaderWorker(QObject *parent)
    : QObject(parent)
{
}

void ClipReaderWorker::openPath(const QString &path)
{
    QMutexLocker lock(&m_mutex);
    m_reader.open(path);
}

void ClipReaderWorker::closePath()
{
    QMutexLocker lock(&m_mutex);
    m_reader.close();
}

QImage ClipReaderWorker::decodeVideo(drift::TimeUs sourceUs, int maxWidth, int maxHeight)
{
    QMutexLocker lock(&m_mutex);
    QImage frame;
    if (!m_reader.readVideoFrameAt(sourceUs, frame, maxWidth, maxHeight))
        return {};
    return frame;
}

int ClipReaderWorker::decodeAudio(drift::TimeUs sourceStartUs, int sampleCount, int outputSampleRate,
                                  float *interleavedStereoOut)
{
    QMutexLocker lock(&m_mutex);
    return m_reader.readAudioInterleaved(sourceStartUs, sampleCount, outputSampleRate, interleavedStereoOut);
}

void ClipReaderWorker::prefetchNextVideo(int maxWidth, int maxHeight)
{
    QMutexLocker lock(&m_mutex);
    m_reader.prefetchNextVideoFrame(maxWidth, maxHeight);
}
