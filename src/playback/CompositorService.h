#pragma once

#include "engine/FrameCompositor.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <atomic>

class CompositorWorker : public QObject
{
    Q_OBJECT

public:
    explicit CompositorWorker(QObject *parent = nullptr);

public slots:
    void setProject(const drift::Project *project);
    void composite(drift::TimeUs timeUs, FrameCompositor::RenderOptions options);
    QImage takeLatestFrame() const;

signals:
    void frameReady(const QImage &frame, drift::TimeUs timeUs);

private:
    struct TripleBuffer
    {
        QImage buffers[3];
        std::atomic<int> writeIndex{0};
        std::atomic<int> latestIndex{-1};
        mutable QMutex mutex;

        void publish(QImage frame);
        QImage takeLatest() const;
    };

    FrameCompositor m_compositor;
    TripleBuffer m_buffer;
};

// Background compositor thread with triple-buffered frame delivery to the GUI.
class CompositorService : public QObject
{
    Q_OBJECT

public:
    explicit CompositorService(QObject *parent = nullptr);
    ~CompositorService() override;

    void setProject(const drift::Project *project);
    void requestComposite(drift::TimeUs timeUs,
                          FrameCompositor::RenderOptions options = FrameCompositor::RenderOptions{});
    QImage latestFrame() const;

signals:
    void frameReady(const QImage &frame);

private slots:
    void onWorkerFrameReady(const QImage &frame, drift::TimeUs timeUs);

private:
    std::atomic<bool> m_requestPending{false};
    std::atomic<drift::TimeUs> m_pendingTimeUs{0};
    std::atomic<int> m_pendingPreviewScalePercent{100};
    std::atomic<int> m_pendingMaxTimeEchoHistoryFrames{-1};
    drift::TimeUs m_lastDispatchedTimeUs = -1;
    FrameCompositor::RenderOptions m_lastDispatchedOptions;
    QThread m_thread;
    CompositorWorker *m_worker = nullptr;
};
