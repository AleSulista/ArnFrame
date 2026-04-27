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
    void composite(drift::TimeUs timeUs);
    QImage takeLatestFrame() const;

signals:
    void frameReady(const QImage &frame);

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
    void requestComposite(drift::TimeUs timeUs);
    QImage latestFrame() const;

signals:
    void frameReady(const QImage &frame);

private slots:
    void onWorkerFrameReady(const QImage &frame);

private:
    std::atomic<bool> m_requestPending{false};
    std::atomic<drift::TimeUs> m_pendingTimeUs{0};
    drift::TimeUs m_lastDispatchedTimeUs = -1;
    QThread m_thread;
    CompositorWorker *m_worker = nullptr;
};
