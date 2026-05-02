#pragma once

#include "core/Time.h"

#include <QImage>
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

    bool readVideoFrameAt(drift::TimeUs sourceUs, QImage &out, int targetWidth, int targetHeight);
    int readAudioInterleaved(drift::TimeUs sourceStartUs, int sampleCount, int outputSampleRate,
                             float *interleavedStereoOut);

private:
    bool ensureVideoDecoder();
    bool ensureAudioDecoder();
    bool tryOpenHardwareDecoder();
    bool transferHwFrameToImage(const AVFrame *hwFrame, QImage &out, int targetWidth, int targetHeight);
    bool convertFrame(const AVFrame *frame, QImage &out, int targetWidth, int targetHeight);
    bool seekVideoStream(drift::TimeUs sourceUs);
    bool seekAudioStream(drift::TimeUs sourceUs);

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
    AVPixelFormat m_hwPixFmt = AV_PIX_FMT_NONE;

    // Sequential-decode state: lets playback decode forward without re-seeking
    // to a keyframe on every frame. Only seek on a backward jump or a large gap.
    bool m_videoPositioned = false;
    drift::TimeUs m_lastVideoPtsUs = 0;
    QImage m_lastVideoFrame;
    int m_lastVideoW = 0;
    int m_lastVideoH = 0;
    static constexpr drift::TimeUs kForwardSeekThresholdUs = 2 * drift::kUsPerSecond;
    static constexpr drift::TimeUs kFrameToleranceUs = 40'000; // ~ >1 frame at 25fps

    // Sequential audio decode state (mirrors the video fast-path): keep the
    // resampler and demux position across buffers so contiguous playback decodes
    // straight through instead of re-seeking on every ~10 ms buffer.
    bool m_audioPositioned = false;
    drift::TimeUs m_audioNextPtsUs = 0; // source position of m_audioLeftover front
    QVector<float> m_audioLeftover;     // decoded-but-unreturned interleaved stereo
    static constexpr drift::TimeUs kAudioSeekToleranceUs = 50'000;
    static constexpr drift::TimeUs kAudioForwardSeekThresholdUs = 2 * drift::kUsPerSecond;
};
