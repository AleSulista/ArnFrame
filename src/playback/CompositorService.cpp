#include "CompositorService.h"

#include <QElapsedTimer>
#include <QMetaType>
#include <cmath>

CompositorWorker::CompositorWorker(QObject *parent)
    : QObject(parent)
{
}

void CompositorWorker::composite(drift::TimeUs timeUs, FrameCompositor::RenderOptions options,
                                 std::shared_ptr<const drift::Project> snapshot, int generation)
{
    // Keep the shared tree alive for the whole frame; setProject only borrows.
    m_snapshot = std::move(snapshot);
    if (!m_snapshot)
        return;
    m_compositor.setProject(m_snapshot.get());

    QElapsedTimer timer;
    timer.start();
    const GpuFrameTexture frame = m_compositor.compositeToTextureAt(timeUs, options);
    const qint64 renderMs = timer.elapsed();
    if (frame.isValid())
        emit frameReady(frame, timeUs, renderMs, generation);
}

CompositorService::CompositorService(QObject *parent)
    : QObject(parent)
    , m_worker(new CompositorWorker)
{
    qRegisterMetaType<drift::TimeUs>("drift::TimeUs");
    qRegisterMetaType<std::shared_ptr<const drift::Project>>("std::shared_ptr<const drift::Project>");
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
    m_project = project;
    invalidateSnapshot();
}

void CompositorService::invalidateSnapshot()
{
    ++m_liveGeneration;
    m_sharedSnapshot.reset();
    // A new edit generation must not be blocked by a previous forward frame.
    m_lastPresentedTimeUs = -1;
    m_minPresentableTimeUs = 0;
}

void CompositorService::setFrameBudgetMs(qint64 budgetMs)
{
    m_frameBudgetMs = qMax<qint64>(1, budgetMs);
}

void CompositorService::setAdaptiveEnabled(bool enabled)
{
    m_adaptiveEnabled = enabled;
    if (!enabled)
        resetAdaptiveScale();
}

void CompositorService::resetAdaptiveScale()
{
    m_adaptive = AdaptivePreviewPolicy::State{};
}

void CompositorService::noteSeek(drift::TimeUs playheadUs)
{
    m_lastPresentedTimeUs = -1;
    m_minPresentableTimeUs = qMax<drift::TimeUs>(0, playheadUs);
}

double CompositorService::adaptiveScaleFactor() const
{
    return m_adaptive.scale;
}

FrameCompositor::RenderOptions CompositorService::effectiveOptions(
    FrameCompositor::RenderOptions options) const
{
    const double factor = m_adaptiveEnabled ? m_adaptive.scale : 1.0;
    options.previewScale = qBound(0.1, options.previewScale * factor, 1.0);
    return options;
}

void CompositorService::dispatch(drift::TimeUs timeUs, const FrameCompositor::RenderOptions &options,
                                 int generation)
{
    if (!m_project)
        return;

    if (!m_sharedSnapshot || m_snapshotGeneration != m_liveGeneration) {
        // One uniquely-owned snapshot per generation; subsequent ticks only bump
        // the shared_ptr. Plain Project copy would keep sharing QMap/QList with
        // the live tree — unsafe once the GUI mutates while the worker reads.
        m_sharedSnapshot = std::make_shared<drift::Project>(m_project->detachedCopy());
        m_snapshotGeneration = m_liveGeneration;
    }

    QMetaObject::invokeMethod(m_worker, "composite", Qt::QueuedConnection,
                              Q_ARG(drift::TimeUs, timeUs),
                              Q_ARG(FrameCompositor::RenderOptions, options),
                              Q_ARG(std::shared_ptr<const drift::Project>, m_sharedSnapshot),
                              Q_ARG(int, generation));
}

void CompositorService::requestComposite(drift::TimeUs timeUs, FrameCompositor::RenderOptions options)
{
    options.previewScale = qBound(0.1, options.previewScale, 1.0);
    const int generation = m_liveGeneration;
    m_pendingTimeUs.store(timeUs, std::memory_order_release);
    m_pendingBaseScalePercent.store(
        qBound(10, static_cast<int>(std::lround(options.previewScale * 100.0)), 100),
        std::memory_order_release);
    m_pendingMaxTimeEchoHistoryFrames.store(options.maxTimeEchoHistoryFrames, std::memory_order_release);
    m_pendingGeneration.store(generation, std::memory_order_release);

    options = effectiveOptions(options);
    if (m_requestPending.exchange(true, std::memory_order_acq_rel))
        return;

    m_lastDispatchedTimeUs = timeUs;
    m_lastDispatchedOptions = options;
    m_lastDispatchedGeneration = generation;
    dispatch(timeUs, options, generation);
}

void CompositorService::onWorkerFrameReady(const GpuFrameTexture &frame, drift::TimeUs timeUs,
                                           qint64 renderMs, int generation)
{
    const drift::TimeUs latest = m_pendingTimeUs.load(std::memory_order_acquire);
    const int pendingGeneration = m_pendingGeneration.load(std::memory_order_acquire);
    FrameCompositor::RenderOptions latestOptions;
    latestOptions.previewScale =
        static_cast<double>(m_pendingBaseScalePercent.load(std::memory_order_acquire)) / 100.0;
    latestOptions.maxTimeEchoHistoryFrames =
        m_pendingMaxTimeEchoHistoryFrames.load(std::memory_order_acquire);

    if (m_adaptiveEnabled && m_frameBudgetMs > 0)
        m_adaptive = AdaptivePreviewPolicy::noteRenderCost(m_adaptive, renderMs, m_frameBudgetMs);

    // CapCut/Premiere behaviour: keep showing the newest completed forward frame
    // so the preview never freezes while the worker catches up. Only reject
    // obsolete edit/seek generations or reverse seeks.
    if (AdaptivePreviewPolicy::shouldPresentFrame(timeUs, generation, m_lastPresentedTimeUs,
                                                   m_liveGeneration, m_minPresentableTimeUs)) {
        m_lastPresentedTimeUs = timeUs;
        emit frameReady(frame);
    }

    m_requestPending.store(false, std::memory_order_release);

    latestOptions = effectiveOptions(latestOptions);
    if (latest == m_lastDispatchedTimeUs
        && pendingGeneration == m_lastDispatchedGeneration
        && latestOptions.previewScale == m_lastDispatchedOptions.previewScale
        && latestOptions.maxTimeEchoHistoryFrames == m_lastDispatchedOptions.maxTimeEchoHistoryFrames)
        return;

    // Catch-up uses the newly adapted scale so the next frame is cheaper.
    m_lastDispatchedTimeUs = latest;
    m_lastDispatchedOptions = latestOptions;
    m_lastDispatchedGeneration = pendingGeneration;
    if (m_requestPending.exchange(true, std::memory_order_acq_rel))
        return;

    dispatch(latest, latestOptions, pendingGeneration);
}
