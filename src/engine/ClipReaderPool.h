#pragma once

#include "core/Time.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThread>

#include <map>
#include <memory>

class ClipReaderWorker;

// Threaded reader pool: one worker thread per media path (video and audio are separate).
class ClipReaderPool
{
public:
    static ClipReaderPool &instance();

    QImage readVideoFrame(const QString &path, drift::TimeUs sourceUs, int targetWidth, int targetHeight);
    int readAudioInterleaved(const QString &path, drift::TimeUs sourceStartUs, int sampleCount,
                             int outputSampleRate, float *interleavedStereoOut);
    void prefetchVideo(const QString &path, drift::TimeUs sourceUs, int targetWidth, int targetHeight);
    void retainActivePaths(const QSet<QString> &videoPaths, const QSet<QString> &audioPaths);

private:
    ClipReaderPool() = default;
    ~ClipReaderPool();

    struct WorkerEntry
    {
        std::unique_ptr<QThread> thread;
        ClipReaderWorker *worker = nullptr;
    };

    static void stopWorkerEntry(WorkerEntry &entry);
    ClipReaderWorker *ensureWorker(std::map<QString, std::unique_ptr<WorkerEntry>> &workers, const QString &path);

    QMutex m_mutex;
    std::map<QString, std::unique_ptr<WorkerEntry>> m_videoWorkers;
    std::map<QString, std::unique_ptr<WorkerEntry>> m_audioWorkers;
};
