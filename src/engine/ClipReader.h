#pragma once

#include "core/Time.h"

#include <QImage>
#include <QString>

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
};
