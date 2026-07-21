#pragma once

#include "engine/FrameCompositor.h"
#include "engine/GpuCompositor.h"

#include <QObject>
#include <QThread>
#include <atomic>

class CompositorWorker : public QObject
{
    Q_OBJECT

public:
    explicit CompositorWorker(QObject *parent = nullptr);

public slots:
    // The project arrives by value, as a snapshot taken on the GUI thread. The worker must
    // never hold a pointer into the live project: compositing runs concurrently with editing,
    // and reading a QMap/QList while the GUI thread rebalances it is a use-after-free.
    void composite(drift::TimeUs timeUs, FrameCompositor::RenderOptions options,
                   drift::Project snapshot);

signals:
    void frameReady(const GpuFrameTexture &frame, drift::TimeUs timeUs);

private:
    FrameCompositor m_compositor;
    // Owns the frame's view of the project for as long as it is being composited. Project is
    // built entirely from Qt's copy-on-write containers, so holding it costs refcounts, not
    // copies — and a concurrent edit detaches into a new tree instead of mutating this one.
    drift::Project m_snapshot;
};

// Background compositor thread. Frames are delivered to the GUI as live GL
// textures out of the runtime's presentation ring — never read back to the CPU
// and never re-uploaded. The ring is what keeps the scene graph from sampling a
// target that is being drawn into, so there is no frame buffering here.
class CompositorService : public QObject
{
    Q_OBJECT

public:
    explicit CompositorService(QObject *parent = nullptr);
    ~CompositorService() override;

    void setProject(const drift::Project *project);
    void requestComposite(drift::TimeUs timeUs,
                          FrameCompositor::RenderOptions options = FrameCompositor::RenderOptions{});

signals:
    void frameReady(const GpuFrameTexture &frame);

private slots:
    void onWorkerFrameReady(const GpuFrameTexture &frame, drift::TimeUs timeUs);

private:
    // Dispatches a composite carrying a fresh snapshot. GUI thread only — that is what makes
    // taking the copy safe.
    void dispatch(drift::TimeUs timeUs, const FrameCompositor::RenderOptions &options);

    // Live project, read only on the GUI thread to take snapshots from.
    const drift::Project *m_project = nullptr;
    std::atomic<bool> m_requestPending{false};
    std::atomic<drift::TimeUs> m_pendingTimeUs{0};
    std::atomic<int> m_pendingPreviewScalePercent{100};
    std::atomic<int> m_pendingMaxTimeEchoHistoryFrames{-1};
    drift::TimeUs m_lastDispatchedTimeUs = -1;
    FrameCompositor::RenderOptions m_lastDispatchedOptions;
    QThread m_thread;
    CompositorWorker *m_worker = nullptr;
};

Q_DECLARE_METATYPE(GpuFrameTexture)
Q_DECLARE_METATYPE(drift::Project)
