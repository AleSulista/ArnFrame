#include "CompositorService.h"

#include <QMetaType>
#include <cmath>

namespace {
constexpr drift::TimeUs kMaxPreviewFrameStalenessUs = 100'000;
}

CompositorWorker::CompositorWorker(QObject *parent)
    : QObject(parent)
{
}

void CompositorWorker::composite(drift::TimeUs timeUs, FrameCompositor::RenderOptions options,
                                 drift::Project snapshot)
{
    // Take ownership before compositing, so the tree being walked is one nothing else holds a
    // mutable reference to. Releasing the previous snapshot here also keeps it alive for the
    // whole of the frame that used it.
    m_snapshot = std::move(snapshot);
    m_compositor.setProject(&m_snapshot);

    const GpuFrameTexture frame = m_compositor.compositeToTextureAt(timeUs, options);
    if (frame.isValid())
        emit frameReady(frame, timeUs);
}

CompositorService::CompositorService(QObject *parent)
    : QObject(parent)
    , m_worker(new CompositorWorker)
{
    qRegisterMetaType<drift::TimeUs>("drift::TimeUs");
    qRegisterMetaType<drift::Project>("drift::Project");
    qRegisterMetaType<FrameCompositor::RenderOptions>("FrameCompositor::RenderOptions");
    qRegisterMetaType<GpuFrameTexture>("GpuFrameTexture");
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
    // Kept on this side only. Each composite request carries its own snapshot, so the worker
    // never learns the live project's address.
    m_project = project;
}

void CompositorService::dispatch(drift::TimeUs timeUs, const FrameCompositor::RenderOptions &options)
{
    if (!m_project)
        return;
    QMetaObject::invokeMethod(m_worker, "composite", Qt::QueuedConnection,
                              Q_ARG(drift::TimeUs, timeUs),
                              Q_ARG(FrameCompositor::RenderOptions, options),
                              Q_ARG(drift::Project, *m_project));
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
    dispatch(timeUs, options);
}

void CompositorService::onWorkerFrameReady(const GpuFrameTexture &frame, drift::TimeUs timeUs)
{
    const drift::TimeUs latest = m_pendingTimeUs.load(std::memory_order_acquire);
    FrameCompositor::RenderOptions latestOptions;
    latestOptions.previewScale =
        static_cast<double>(m_pendingPreviewScalePercent.load(std::memory_order_acquire)) / 100.0;
    latestOptions.maxTimeEchoHistoryFrames = m_pendingMaxTimeEchoHistoryFrames.load(std::memory_order_acquire);

    const bool stale = latest > timeUs && latest - timeUs > kMaxPreviewFrameStalenessUs;
    if (!stale)
        emit frameReady(frame);

    m_requestPending.store(false, std::memory_order_release);

    if (latest == m_lastDispatchedTimeUs
        && latestOptions.previewScale == m_lastDispatchedOptions.previewScale
        && latestOptions.maxTimeEchoHistoryFrames == m_lastDispatchedOptions.maxTimeEchoHistoryFrames)
        return;

    m_lastDispatchedTimeUs = latest;
    m_lastDispatchedOptions = latestOptions;
    if (m_requestPending.exchange(true, std::memory_order_acq_rel))
        return;

    dispatch(latest, latestOptions);
}
