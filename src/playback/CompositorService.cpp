#include "CompositorService.h"

#include <QMetaType>
#include <cmath>

void CompositorWorker::TripleBuffer::publish(QImage frame)
{
    QMutexLocker lock(&mutex);
    const int idx = writeIndex.load(std::memory_order_relaxed) % 3;
    buffers[idx] = std::move(frame);
    latestIndex.store(idx, std::memory_order_release);
    writeIndex.store((idx + 1) % 3, std::memory_order_relaxed);
}

QImage CompositorWorker::TripleBuffer::takeLatest() const
{
    QMutexLocker lock(&mutex);
    const int idx = latestIndex.load(std::memory_order_acquire);
    if (idx < 0 || idx > 2)
        return {};
    return buffers[idx];
}

CompositorWorker::CompositorWorker(QObject *parent)
    : QObject(parent)
{
}

void CompositorWorker::setProject(const drift::Project *project)
{
    m_compositor.setProject(project);
}

void CompositorWorker::composite(drift::TimeUs timeUs, FrameCompositor::RenderOptions options)
{
    const QImage frame = m_compositor.compositeAt(timeUs, options);
    m_buffer.publish(frame);
    emit frameReady(frame);
}

QImage CompositorWorker::takeLatestFrame() const
{
    return m_buffer.takeLatest();
}

CompositorService::CompositorService(QObject *parent)
    : QObject(parent)
    , m_worker(new CompositorWorker)
{
    qRegisterMetaType<drift::TimeUs>("drift::TimeUs");
    qRegisterMetaType<const drift::Project *>("const drift::Project*");
    qRegisterMetaType<FrameCompositor::RenderOptions>("FrameCompositor::RenderOptions");
    m_worker->moveToThread(&m_thread);
    connect(m_worker, &CompositorWorker::frameReady, this, &CompositorService::onWorkerFrameReady,
            Qt::QueuedConnection);
    m_thread.start();
}

CompositorService::~CompositorService()
{
    m_thread.quit();
    m_thread.wait();
    delete m_worker;
    m_worker = nullptr;
}

void CompositorService::setProject(const drift::Project *project)
{
    QMetaObject::invokeMethod(m_worker, "setProject", Qt::BlockingQueuedConnection,
                              Q_ARG(const drift::Project *, project));
}

void CompositorService::requestComposite(drift::TimeUs timeUs, FrameCompositor::RenderOptions options)
{
    options.previewScale = qBound(0.1, options.previewScale, 1.0);
    m_pendingTimeUs.store(timeUs, std::memory_order_release);
    m_pendingPreviewScalePercent.store(qBound(10, static_cast<int>(std::lround(options.previewScale * 100.0)), 100),
                                       std::memory_order_release);
    m_pendingMaxTimeEchoHistoryFrames.store(options.maxTimeEchoHistoryFrames, std::memory_order_release);
    if (m_requestPending.exchange(true, std::memory_order_acq_rel))
        return;

    m_lastDispatchedTimeUs = timeUs;
    m_lastDispatchedOptions = options;
    QMetaObject::invokeMethod(m_worker, "composite", Qt::QueuedConnection, Q_ARG(drift::TimeUs, timeUs),
                              Q_ARG(FrameCompositor::RenderOptions, options));
}

void CompositorService::onWorkerFrameReady(const QImage &frame)
{
    emit frameReady(frame);
    m_requestPending.store(false, std::memory_order_release);

    const drift::TimeUs latest = m_pendingTimeUs.load(std::memory_order_acquire);
    FrameCompositor::RenderOptions latestOptions;
    latestOptions.previewScale =
        static_cast<double>(m_pendingPreviewScalePercent.load(std::memory_order_acquire)) / 100.0;
    latestOptions.maxTimeEchoHistoryFrames = m_pendingMaxTimeEchoHistoryFrames.load(std::memory_order_acquire);

    if (latest == m_lastDispatchedTimeUs
        && latestOptions.previewScale == m_lastDispatchedOptions.previewScale
        && latestOptions.maxTimeEchoHistoryFrames == m_lastDispatchedOptions.maxTimeEchoHistoryFrames)
        return;

    m_lastDispatchedTimeUs = latest;
    m_lastDispatchedOptions = latestOptions;
    if (m_requestPending.exchange(true, std::memory_order_acq_rel))
        return;

    QMetaObject::invokeMethod(m_worker, "composite", Qt::QueuedConnection, Q_ARG(drift::TimeUs, latest),
                              Q_ARG(FrameCompositor::RenderOptions, latestOptions));
}

QImage CompositorService::latestFrame() const
{
    QImage frame;
    QMetaObject::invokeMethod(m_worker, "takeLatestFrame", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QImage, frame));
    return frame;
}
