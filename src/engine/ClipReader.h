#pragma once

#include "core/Time.h"

#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <QVector>

extern "C" {
#include <libavutil/pixfmt.h>
struct AVFrame;
}

// Threaded-capable demux/decode for a single media file.
// Opens its own AVFormatContext; seeks via keyframe + forward decode.
class ClipReader
{
public:
    ClipReader();
    ~ClipReader();

    ClipReader(const ClipReader &) = delete;
    ClipReader &operator=(const ClipReader &) = delete;

    bool open(const QString &path);
    void close();
    bool isOpen() const { return m_fmt != nullptr; }
    const QString &path() const { return m_path; }

    bool hasVideo() const { return m_videoStream >= 0; }
    bool hasAudio() const { return m_audioStream >= 0; }

    // maxWidth/maxHeight bound the decode buffer; they are a hint, not an exact
    // size. The reader fits the source into that box (never upscaling) and keeps
    // the resulting decode size stable, so an animated clip transform does not
    // change the decode size — and therefore does not invalidate the frame cache
    // — on every frame. Callers scale the returned image to their layout rect.
    bool readVideoFrameAt(drift::TimeUs sourceUs, QImage &out, int maxWidth, int maxHeight);
    // Decode one frame past the current position into the cache, to overlap
    // decode with the caller's compositing work.
    void prefetchNextVideoFrame(int maxWidth, int maxHeight);
    int readAudioInterleaved(drift::TimeUs sourceStartUs, int sampleCount, int outputSampleRate,
                             float *interleavedStereoOut);

private:
    bool ensureVideoDecoder();
    bool ensureAudioDecoder();
    bool openSoftwareVideoDecoder();
    bool tryOpenHardwareDecoder();
    void teardownVideoDecoder();
    bool fallbackFromHardwareDecoder();
    bool transferHwFrameToImage(const AVFrame *hwFrame, QImage &out, int targetWidth, int targetHeight);
    bool convertFrame(const AVFrame *frame, QImage &out, int targetWidth, int targetHeight);
    bool decodeVideoFrameAtOnce(drift::TimeUs sourceUs, QImage &out, int maxWidth, int maxHeight,
                                bool *hwFailure);
    bool seekVideoStream(drift::TimeUs sourceUs);
    bool seekAudioStream(drift::TimeUs sourceUs);

    // Decode size fitted into the caller's box, quantized so small preview
    // resizes don't churn the sws context and the frame cache.
    QSize decodeSizeFor(int maxWidth, int maxHeight) const;
    void applyDecodeSize(const QSize &size);
    drift::TimeUs frameToleranceUs() const;
    bool lookupCachedFrame(drift::TimeUs sourceUs, QImage &out) const;
    void storeCachedFrame(drift::TimeUs ptsUs, const QImage &image);

    QString m_path;
    struct AVFormatContext *m_fmt = nullptr;
    struct AVCodecContext *m_videoCtx = nullptr;
    struct AVCodecContext *m_audioCtx = nullptr;
    struct AVBufferRef *m_hwDeviceCtx = nullptr;
    struct SwsContext *m_sws = nullptr;
    struct SwrContext *m_swr = nullptr;
    int m_videoStream = -1;
    int m_audioStream = -1;
    int m_outputSampleRate = 48000;
    bool m_hwAccelActive = false;
    bool m_hwAccelDisabled = false; // sticky after a failed VAAPI decode
    AVPixelFormat m_hwPixFmt = AV_PIX_FMT_NONE;

    // Sequential-decode state: lets playback decode forward without re-seeking
    // to a keyframe on every frame. Only seek on a backward jump or a large gap.
    bool m_videoPositioned = false;
    drift::TimeUs m_lastVideoPtsUs = 0;
    int m_decodeW = 0;
    int m_decodeH = 0;
    drift::TimeUs m_sourceFrameDurationUs = 0; // 0 until the stream is opened
    static constexpr drift::TimeUs kForwardSeekThresholdUs = 2 * drift::kUsPerSecond;

    // Recently returned frames, most-recent first, keyed by source PTS. Backward
    // scrubbing and time_echo's history samples hit this instead of re-seeking.
    struct CachedFrame
    {
        drift::TimeUs ptsUs = 0;
        QImage image;
    };
    QList<CachedFrame> m_videoCache;
    static constexpr int kMaxCachedFrames = 16;

    // Sequential audio decode state (mirrors the video fast-path): keep the
    // resampler and demux position across buffers so contiguous playback decodes
    // straight through instead of re-seeking on every ~10 ms buffer.
    bool m_audioPositioned = false;
    drift::TimeUs m_audioNextPtsUs = 0; // source position of m_audioLeftover front
    QVector<float> m_audioLeftover;     // decoded-but-unreturned interleaved stereo
    static constexpr drift::TimeUs kAudioSeekToleranceUs = 50'000;
    static constexpr drift::TimeUs kAudioForwardSeekThresholdUs = 2 * drift::kUsPerSecond;
};
