#pragma once

#include "ClipReader.h"
#include "core/Time.h"

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThread>

#include <QList>

#include <atomic>
#include <map>
#include <memory>

class ClipReaderWorker;

// Threaded reader pool: one worker thread per media path (video and audio are separate). Each
// worker holds a decoder per stream id, so callers reading the same file at different positions do
// not fight over one decode cursor — see ClipReaderWorker.
class ClipReaderPool
{
public:
    static ClipReaderPool &instance();

    // A stream id is one caller's decode cursor on a path: one per timeline clip, per preview
    // player, per offline scan. Two callers sharing one is what made overlapping clips cut from a
    // single file desync their audio and reseek their video on every frame.
    static quint64 streamIdForClip(const QString &clipId) { return qHash(clipId); }

    struct VideoRequest
    {
        QString path;
        quint64 streamId = 0;
        drift::TimeUs sourceUs = 0;
        int maxWidth = 0;
        int maxHeight = 0;
    };

    // Kick every request off on its own worker thread without waiting. Each path
    // has its own thread, so the decodes run concurrently; the readVideoFrame
    // calls that follow then hit each reader's cache instead of decoding one
    // clip after another on the caller's thread.
    void warmVideoFrames(const QList<VideoRequest> &requests);

    // How far past the frame being composited each reader keeps decoding, in
    // source time. Set per composite from the render options; 0 (the default)
    // leaves the plain one-frame-ahead prefetch. Only the NV12 preview path
    // buffers — export and thumbnails consume as fast as they decode anyway.
    void setReadAheadUs(drift::TimeUs readAheadUs);

    // Preview toolbar: Auto (per clip), Software, or Hardware on a named backend.
    // Drops every open video decoder so the next read opens on the chosen path.
    void setHardwareDecodeMode(ClipReader::HardwareDecodeMode mode,
                               drift::hwaccel::Backend backend = drift::hwaccel::Backend::None);

    QImage readVideoFrame(const QString &path, quint64 streamId, drift::TimeUs sourceUs, int maxWidth,
                          int maxHeight);
    // Preview path: NV12 for cheaper GPU upload. Falls back empty when decode fails.
    Nv12Frame readVideoFrameNv12(const QString &path, quint64 streamId, drift::TimeUs sourceUs,
                                 int maxWidth, int maxHeight);
    int readAudioInterleaved(const QString &path, quint64 streamId, drift::TimeUs sourceStartUs,
                             int sampleCount, int outputSampleRate, float *interleavedStereoOut);
    // Tell every audio reader its cursor is stale, so the next read seeks to the position asked
    // for. Called when the timeline playhead moves: a short forward seek looks like ordinary
    // playback to the sequential fast path, which would keep streaming from the old position.
    void resetAudioStreams();
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
    ClipReaderWorker *ensureWorker(std::map<QString, std::unique_ptr<WorkerEntry>> &workers,
                                   const QString &path);

    QMutex m_mutex;
    std::atomic<drift::TimeUs> m_readAheadUs{0};
    std::map<QString, std::unique_ptr<WorkerEntry>> m_videoWorkers;
    std::map<QString, std::unique_ptr<WorkerEntry>> m_audioWorkers;
};
