#include "ClipReaderWorker.h"

ClipReaderWorker::ClipReaderWorker(QObject *parent)
    : QObject(parent)
{
}

void ClipReaderWorker::openPath(const QString &path, bool audioOnly)
{
    QMutexLocker lock(&m_mutex);
    m_path = path;
    if (!audioOnly)
        m_reader.open(path);
}

void ClipReaderWorker::closePath()
{
    QMutexLocker lock(&m_mutex);
    m_reader.close();
    m_audioReaders.clear();
    m_audioLru.clear();
}

QImage ClipReaderWorker::decodeVideo(drift::TimeUs sourceUs, int maxWidth, int maxHeight)
{
    QMutexLocker lock(&m_mutex);
    QImage frame;
    if (!m_reader.readVideoFrameAt(sourceUs, frame, maxWidth, maxHeight))
        return {};
    return frame;
}

Nv12Frame ClipReaderWorker::decodeVideoNv12(drift::TimeUs sourceUs, int maxWidth, int maxHeight)
{
    QMutexLocker lock(&m_mutex);
    Nv12Frame frame;
    if (!m_reader.readVideoFrameAtNv12(sourceUs, frame, maxWidth, maxHeight))
        return {};
    return frame;
}

// Runs on the worker thread, so opening and closing readers here never blocks the audio callback
// on an avformat operation.
ClipReader *ClipReaderWorker::audioReaderFor(quint64 streamId)
{
    auto it = m_audioReaders.find(streamId);
    if (it == m_audioReaders.end()) {
        if (m_path.isEmpty())
            return nullptr;
        auto reader = std::make_unique<ClipReader>();
        if (!reader->open(m_path))
            return nullptr;
        it = m_audioReaders.emplace(streamId, std::move(reader)).first;
    }

    std::erase(m_audioLru, streamId);
    m_audioLru.push_back(streamId);
    while (m_audioLru.size() > kMaxAudioStreams) {
        m_audioReaders.erase(m_audioLru.front());
        m_audioLru.erase(m_audioLru.begin());
    }

    return it->second.get();
}

int ClipReaderWorker::decodeAudio(quint64 streamId, drift::TimeUs sourceStartUs, int sampleCount,
                                  int outputSampleRate, float *interleavedStereoOut)
{
    QMutexLocker lock(&m_mutex);

    if (m_audioRepositionPending.fetchAndStoreAcquire(0) != 0) {
        for (auto &entry : m_audioReaders)
            entry.second->invalidateAudioPosition();
    }

    ClipReader *reader = audioReaderFor(streamId);
    if (!reader)
        return 0;
    return reader->readAudioInterleaved(sourceStartUs, sampleCount, outputSampleRate,
                                        interleavedStereoOut);
}

void ClipReaderWorker::prefetchNextVideo(int maxWidth, int maxHeight)
{
    QMutexLocker lock(&m_mutex);
    m_reader.prefetchNextVideoFrame(maxWidth, maxHeight);
}

void ClipReaderWorker::requestPrefetchNv12(int maxWidth, int maxHeight, drift::TimeUs readAheadUs)
{
    if (!m_prefetchPending.testAndSetAcquire(0, 1))
        return;

    QMetaObject::invokeMethod(this, "prefetchNextVideoNv12", Qt::QueuedConnection,
                              Q_ARG(int, maxWidth), Q_ARG(int, maxHeight),
                              Q_ARG(drift::TimeUs, readAheadUs));
}

void ClipReaderWorker::prefetchNextVideoNv12(int maxWidth, int maxHeight, drift::TimeUs readAheadUs)
{
    m_prefetchPending.storeRelease(0);

    bool more = false;
    {
        QMutexLocker lock(&m_mutex);
        more = m_reader.prefetchNextVideoFrameNv12(maxWidth, maxHeight, readAheadUs);
    }

    // Re-post instead of looping: this yields the event queue and the reader
    // mutex between frames, so a decode request that arrives mid-buffer waits
    // for one frame rather than for the whole read-ahead.
    if (more)
        requestPrefetchNv12(maxWidth, maxHeight, readAheadUs);
}
