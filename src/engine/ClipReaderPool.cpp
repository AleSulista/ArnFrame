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

QImage ClipReaderPool::readVideoFrame(const QString &path, drift::TimeUs sourceUs, int targetWidth,
                                      int targetHeight)
{
    if (path.isEmpty())
        return {};

    QMutexLocker lock(&m_mutex);
    ClipReaderWorker *worker = ensureWorker(m_videoWorkers, path);

    QImage frame;
    QMetaObject::invokeMethod(worker, "decodeVideo", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QImage, frame),
                              Q_ARG(drift::TimeUs, sourceUs), Q_ARG(int, targetWidth), Q_ARG(int, targetHeight));

    const drift::TimeUs nextUs = sourceUs + drift::kUsPerSecond / 30;
    QMetaObject::invokeMethod(worker, "prefetchVideo", Qt::QueuedConnection, Q_ARG(drift::TimeUs, nextUs),
                              Q_ARG(int, targetWidth), Q_ARG(int, targetHeight));

    return frame;
}

int ClipReaderPool::readAudioInterleaved(const QString &path, drift::TimeUs sourceStartUs, int sampleCount,
                                         int outputSampleRate, float *interleavedStereoOut)
{
    if (path.isEmpty() || !interleavedStereoOut || sampleCount <= 0)
        return 0;

    QMutexLocker lock(&m_mutex);
    ClipReaderWorker *worker = ensureWorker(m_audioWorkers, path);

    int written = 0;
    QMetaObject::invokeMethod(worker, "decodeAudio", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, written),
                              Q_ARG(drift::TimeUs, sourceStartUs), Q_ARG(int, sampleCount),
                              Q_ARG(int, outputSampleRate), Q_ARG(float *, interleavedStereoOut));
    return written;
}

void ClipReaderPool::prefetchVideo(const QString &path, drift::TimeUs sourceUs, int targetWidth, int targetHeight)
{
    if (path.isEmpty())
        return;

    QMutexLocker lock(&m_mutex);
    ClipReaderWorker *worker = ensureWorker(m_videoWorkers, path);
    QMetaObject::invokeMethod(worker, "prefetchVideo", Qt::QueuedConnection, Q_ARG(drift::TimeUs, sourceUs),
                              Q_ARG(int, targetWidth), Q_ARG(int, targetHeight));
}

void ClipReaderPool::retainActivePaths(const QSet<QString> &videoPaths, const QSet<QString> &audioPaths)
{
    QMutexLocker lock(&m_mutex);
    for (const QString &path : videoPaths)
        ensureWorker(m_videoWorkers, path);
    for (const QString &path : audioPaths)
        ensureWorker(m_audioWorkers, path);
}
