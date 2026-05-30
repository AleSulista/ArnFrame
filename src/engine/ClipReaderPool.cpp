#include "ClipReaderPool.h"

#include "ClipReaderWorker.h"

#include <QMetaObject>
#include <QMetaType>

ClipReaderPool &ClipReaderPool::instance()
{
    static ClipReaderPool pool;
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<drift::TimeUs>("drift::TimeUs");
        registered = true;
    }
    return pool;
}

ClipReaderPool::~ClipReaderPool()
{
    QMutexLocker lock(&m_mutex);
    for (auto &entry : m_videoWorkers)
        stopWorkerEntry(*entry.second);
    for (auto &entry : m_audioWorkers)
        stopWorkerEntry(*entry.second);
    m_videoWorkers.clear();
    m_audioWorkers.clear();
}

void ClipReaderPool::stopWorkerEntry(WorkerEntry &entry)
{
    if (!entry.thread)
        return;

    if (entry.worker) {
        QMetaObject::invokeMethod(entry.worker, "closePath", Qt::BlockingQueuedConnection);
    }

    entry.thread->quit();
    entry.thread->wait();
    delete entry.worker;
    entry.worker = nullptr;
    entry.thread.reset();
}

ClipReaderWorker *ClipReaderPool::ensureWorker(std::map<QString, std::unique_ptr<WorkerEntry>> &workers,
                                               const QString &path)
{
    auto it = workers.find(path);
    if (it == workers.end()) {
        auto entry = std::make_unique<WorkerEntry>();
        entry->thread = std::make_unique<QThread>();
        entry->worker = new ClipReaderWorker;
        entry->worker->moveToThread(entry->thread.get());
        entry->thread->start();

        QMetaObject::invokeMethod(entry->worker, "openPath", Qt::BlockingQueuedConnection, Q_ARG(QString, path));
        it = workers.emplace(path, std::move(entry)).first;
    }

    return it->second->worker;
}

void ClipReaderPool::warmVideoFrames(const QList<VideoRequest> &requests)
{
    for (const VideoRequest &request : requests) {
        if (request.path.isEmpty())
            continue;

        ClipReaderWorker *worker = nullptr;
        {
            QMutexLocker lock(&m_mutex);
            worker = ensureWorker(m_videoWorkers, request.path);
        }
        // Fire and forget: the worker decodes into its reader's cache on its own
        // thread while we queue the next one.
        QMetaObject::invokeMethod(worker, "decodeVideo", Qt::QueuedConnection,
                                  Q_ARG(drift::TimeUs, request.sourceUs), Q_ARG(int, request.maxWidth),
                                  Q_ARG(int, request.maxHeight));
    }
}

QImage ClipReaderPool::readVideoFrame(const QString &path, drift::TimeUs sourceUs, int maxWidth,
                                      int maxHeight)
{
    if (path.isEmpty())
        return {};

    ClipReaderWorker *worker = nullptr;
    {
        // Hold the pool mutex only to resolve the worker; releasing it before the
        // blocking decode lets audio and video (different workers) decode in
        // parallel instead of serializing on this lock.
        QMutexLocker lock(&m_mutex);
        worker = ensureWorker(m_videoWorkers, path);
    }

    QImage frame;
    QMetaObject::invokeMethod(worker, "decodeVideo", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QImage, frame),
                              Q_ARG(drift::TimeUs, sourceUs), Q_ARG(int, maxWidth), Q_ARG(int, maxHeight));

    // Decode one frame beyond the current position while the caller composites
    // this one. The reader knows the source frame duration; the old code guessed
    // a hardcoded 1/30 s, which missed on every clip that isn't 30 fps.
    QMetaObject::invokeMethod(worker, "prefetchNextVideo", Qt::QueuedConnection, Q_ARG(int, maxWidth),
                              Q_ARG(int, maxHeight));

    return frame;
}

int ClipReaderPool::readAudioInterleaved(const QString &path, drift::TimeUs sourceStartUs, int sampleCount,
                                         int outputSampleRate, float *interleavedStereoOut)
{
    if (path.isEmpty() || !interleavedStereoOut || sampleCount <= 0)
        return 0;

    ClipReaderWorker *worker = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        worker = ensureWorker(m_audioWorkers, path);
    }

    int written = 0;
    QMetaObject::invokeMethod(worker, "decodeAudio", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, written),
                              Q_ARG(drift::TimeUs, sourceStartUs), Q_ARG(int, sampleCount),
                              Q_ARG(int, outputSampleRate), Q_ARG(float *, interleavedStereoOut));
    return written;
}

void ClipReaderPool::retainActivePaths(const QSet<QString> &videoPaths, const QSet<QString> &audioPaths)
{
    QMutexLocker lock(&m_mutex);
    for (const QString &path : videoPaths)
        ensureWorker(m_videoWorkers, path);
    for (const QString &path : audioPaths)
        ensureWorker(m_audioWorkers, path);
}
