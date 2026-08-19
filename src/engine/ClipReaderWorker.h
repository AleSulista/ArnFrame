#pragma once

#include "ClipReader.h"

#include "core/Time.h"

#include <QAtomicInt>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>

#include <map>
#include <memory>
#include <vector>

// Owns a ClipReader on a dedicated thread; all decode calls are serialized here.
// The reader keeps its own frame cache, so this class holds no cache of its own.
class ClipReaderWorker : public QObject
{
    Q_OBJECT

public:
    explicit ClipReaderWorker(QObject *parent = nullptr);

    // Callable from any thread. Queues one read-ahead step if none is pending;
    // that step re-arms itself until the reader has readAheadUs of decoded source
    // buffered. Keeping a single step in flight is what bounds a decode request's
    // wait to one frame — a queue of them would serialize ahead of it.
    void requestPrefetchNv12(int maxWidth, int maxHeight, drift::TimeUs readAheadUs);

    // Callable from any thread. Marks every audio reader here as unpositioned, so the next decode
    // seeks to the position it is asked for instead of continuing its stream. Set as a flag rather
    // than applied directly: the GUI thread raises it on seek while the audio thread may be mid
    // decode, and a blocking call across that boundary would stall the seek behind the decode.
    void requestAudioReposition() { m_audioRepositionPending.storeRelease(1); }

public slots:
    // Audio workers never touch m_reader — their readers live in m_audioReaders, one per stream —
    // so audioOnly keeps them from holding a second, unused AVFormatContext open per media file.
    void openPath(const QString &path, bool audioOnly);
    void closePath();
    QImage decodeVideo(drift::TimeUs sourceUs, int maxWidth, int maxHeight);
    Nv12Frame decodeVideoNv12(drift::TimeUs sourceUs, int maxWidth, int maxHeight);
    // streamId identifies the caller's audio stream: one decode cursor per timeline clip, per
    // preview player, per offline scan. Sharing one cursor between two consumers of the same file
    // silently hands the second one the first one's audio, because the sequential fast path in
    // ClipReader treats a nearby request as a continuation.
    int decodeAudio(quint64 streamId, drift::TimeUs sourceStartUs, int sampleCount,
                    int outputSampleRate, float *interleavedStereoOut);
    void prefetchNextVideo(int maxWidth, int maxHeight);
    void prefetchNextVideoNv12(int maxWidth, int maxHeight, drift::TimeUs readAheadUs);

private:
    // Only clips overlapping right now need concurrent cursors, and that is a handful at most.
    // Past the cap the least recently used reader is closed, which costs one seek if it comes back.
    static constexpr size_t kMaxAudioStreams = 4;

    ClipReader *audioReaderFor(quint64 streamId);

    ClipReader m_reader;
    QString m_path;
    std::map<quint64, std::unique_ptr<ClipReader>> m_audioReaders;
    std::vector<quint64> m_audioLru; // most recently used last
    QMutex m_mutex;
    QAtomicInt m_prefetchPending{0};
    QAtomicInt m_audioRepositionPending{0};
};
