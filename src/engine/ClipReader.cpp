#include "ClipReader.h"

#include <QtMath>

#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace {

thread_local AVPixelFormat g_hwPixFmt = AV_PIX_FMT_NONE;

AVPixelFormat hwGetFormat(AVCodecContext *, const AVPixelFormat *pixFmts)
{
    for (const AVPixelFormat *p = pixFmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == g_hwPixFmt)
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

QImage frameToRgba(const AVFrame *frame, SwsContext *&sws, int targetWidth, int targetHeight)
{
    if (!frame || targetWidth <= 0 || targetHeight <= 0)
        return {};

    sws = sws_getCachedContext(sws, frame->width, frame->height,
                               static_cast<AVPixelFormat>(frame->format), targetWidth, targetHeight,
                               AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws)
        return {};

    AVFrame *rgba = av_frame_alloc();
    if (!rgba)
        return {};

    rgba->format = AV_PIX_FMT_RGBA;
    rgba->width = targetWidth;
    rgba->height = targetHeight;
    if (av_frame_get_buffer(rgba, 0) < 0) {
        av_frame_free(&rgba);
        return {};
    }

    sws_scale(sws, frame->data, frame->linesize, 0, frame->height, rgba->data, rgba->linesize);

    QImage image(rgba->data[0], targetWidth, targetHeight, rgba->linesize[0], QImage::Format_RGBA8888);
    const QImage copy = image.copy();
    av_frame_free(&rgba);
    return copy;
}

drift::TimeUs ptsToUs(const AVFrame *frame, const AVRational &timeBase)
{
    if (!frame || frame->pts == AV_NOPTS_VALUE)
        return 0;
    return av_rescale_q(frame->pts, timeBase, {1, drift::kUsPerSecond});
}

} // namespace

ClipReader::ClipReader() = default;

ClipReader::~ClipReader()
{
    close();
}

void ClipReader::close()
{
    if (m_swr)
        swr_free(&m_swr);
    if (m_sws)
        sws_freeContext(m_sws);
    m_sws = nullptr;

    if (m_videoCtx)
        avcodec_free_context(&m_videoCtx);
    if (m_audioCtx)
        avcodec_free_context(&m_audioCtx);
    if (m_hwDeviceCtx)
        av_buffer_unref(&m_hwDeviceCtx);
    if (m_fmt)
        avformat_close_input(&m_fmt);

    m_videoStream = -1;
    m_audioStream = -1;
    m_hwAccelActive = false;
    m_hwPixFmt = AV_PIX_FMT_NONE;
    m_videoPositioned = false;
    m_lastVideoPtsUs = 0;
    m_lastVideoFrame = {};
    m_lastVideoW = 0;
    m_lastVideoH = 0;
    m_audioPositioned = false;
    m_audioNextPtsUs = 0;
    m_audioLeftover.clear();
    m_path.clear();
}

bool ClipReader::open(const QString &path)
{
    if (path.isEmpty())
        return false;
    if (m_path == path && isOpen())
        return true;

    close();
    m_path = path;

    AVFormatContext *fmt = nullptr;
    if (avformat_open_input(&fmt, path.toUtf8().constData(), nullptr, nullptr) < 0)
        return false;
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return false;
    }

    m_fmt = fmt;
    for (unsigned i = 0; i < m_fmt->nb_streams; ++i) {
        const AVMediaType type = m_fmt->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO && m_videoStream < 0)
            m_videoStream = static_cast<int>(i);
        else if (type == AVMEDIA_TYPE_AUDIO && m_audioStream < 0)
            m_audioStream = static_cast<int>(i);
    }

    return hasVideo() || hasAudio();
}

bool ClipReader::tryOpenHardwareDecoder()
{
    if (!m_fmt || m_videoStream < 0 || m_hwAccelActive)
        return m_hwAccelActive;

    const AVCodecParameters *par = m_fmt->streams[m_videoStream]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec)
        return false;

    for (int i = 0;; ++i) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
        if (!config)
            break;
        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
            && config->device_type == AV_HWDEVICE_TYPE_VAAPI) {
            m_hwPixFmt = config->pix_fmt;
            break;
        }
    }

    if (m_hwPixFmt == AV_PIX_FMT_NONE)
        return false;

    if (av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0) < 0) {
        m_hwPixFmt = AV_PIX_FMT_NONE;
        if (m_hwDeviceCtx)
            av_buffer_unref(&m_hwDeviceCtx);
        return false;
    }

    m_videoCtx = avcodec_alloc_context3(codec);
    if (!m_videoCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwPixFmt = AV_PIX_FMT_NONE;
        return false;
    }

    avcodec_parameters_to_context(m_videoCtx, par);
    m_videoCtx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
    g_hwPixFmt = m_hwPixFmt;
    m_videoCtx->get_format = hwGetFormat;

    if (avcodec_open2(m_videoCtx, codec, nullptr) < 0) {
        avcodec_free_context(&m_videoCtx);
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwPixFmt = AV_PIX_FMT_NONE;
        return false;
    }

    m_hwAccelActive = true;
    return true;
}

bool ClipReader::ensureVideoDecoder()
{
    if (!m_fmt || m_videoStream < 0)
        return false;
    if (m_videoCtx)
        return true;

    if (tryOpenHardwareDecoder())
        return true;

    const AVCodecParameters *par = m_fmt->streams[m_videoStream]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec)
        return false;

    m_videoCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_videoCtx, par);
    return avcodec_open2(m_videoCtx, codec, nullptr) >= 0;
}

bool ClipReader::transferHwFrameToImage(const AVFrame *hwFrame, QImage &out, int targetWidth, int targetHeight)
{
    AVFrame *swFrame = av_frame_alloc();
    if (!swFrame)
        return false;

    if (av_hwframe_transfer_data(swFrame, hwFrame, 0) < 0) {
        av_frame_free(&swFrame);
        return false;
    }

    const QImage image = frameToRgba(swFrame, m_sws, targetWidth, targetHeight);
    av_frame_free(&swFrame);
    if (image.isNull())
        return false;

    out = image;
    return true;
}

bool ClipReader::convertFrame(const AVFrame *frame, QImage &out, int targetWidth, int targetHeight)
{
    if (m_hwAccelActive && frame->format == m_hwPixFmt)
        return transferHwFrameToImage(frame, out, targetWidth, targetHeight);

    const QImage image = frameToRgba(frame, m_sws, targetWidth, targetHeight);
    if (image.isNull())
        return false;
    out = image;
    return true;
}

bool ClipReader::ensureAudioDecoder()
{
    if (!m_fmt || m_audioStream < 0)
        return false;
    if (m_audioCtx)
        return true;

    const AVCodecParameters *par = m_fmt->streams[m_audioStream]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec)
        return false;

    m_audioCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_audioCtx, par);
    return avcodec_open2(m_audioCtx, codec, nullptr) >= 0;
}

bool ClipReader::seekVideoStream(drift::TimeUs sourceUs)
{
    if (!ensureVideoDecoder())
        return false;

    AVStream *stream = m_fmt->streams[m_videoStream];
    const int64_t targetTs = av_rescale_q(sourceUs, {1, AV_TIME_BASE}, stream->time_base);
    if (av_seek_frame(m_fmt, m_videoStream, targetTs, AVSEEK_FLAG_BACKWARD) < 0) {
        if (sourceUs > 0)
            return false;
        av_seek_frame(m_fmt, m_videoStream, 0, AVSEEK_FLAG_BACKWARD);
    }
    avcodec_flush_buffers(m_videoCtx);
    m_videoPositioned = true;
    return true;
}

bool ClipReader::seekAudioStream(drift::TimeUs sourceUs)
{
    if (!ensureAudioDecoder())
        return false;

    AVStream *stream = m_fmt->streams[m_audioStream];
    const int64_t targetTs = av_rescale_q(sourceUs, {1, AV_TIME_BASE}, stream->time_base);
    if (av_seek_frame(m_fmt, m_audioStream, targetTs, AVSEEK_FLAG_BACKWARD) < 0)
        return false;
    avcodec_flush_buffers(m_audioCtx);
    if (m_swr)
        swr_free(&m_swr);
    return true;
}

bool ClipReader::readVideoFrameAt(drift::TimeUs sourceUs, QImage &out, int targetWidth, int targetHeight)
{
    if (!ensureVideoDecoder())
        return false;

    const bool dimsMatch = m_lastVideoW == targetWidth && m_lastVideoH == targetHeight;

    // Serve a re-request of (or tiny overshoot past) the frame we already hold.
    // Playback and scrubbing both re-ask for the current frame constantly.
    if (m_videoPositioned && dimsMatch && !m_lastVideoFrame.isNull()
        && qAbs(sourceUs - m_lastVideoPtsUs) <= kFrameToleranceUs) {
        out = m_lastVideoFrame;
        return true;
    }

    // Fast path: for a forward request within reach of the current decode
    // position, keep decoding forward instead of seeking to a keyframe. Only
    // seek on a backward jump or a large forward gap (a real scrub/jump).
    const bool needSeek = !m_videoPositioned || !dimsMatch
                          || sourceUs < m_lastVideoPtsUs - kFrameToleranceUs
                          || sourceUs - m_lastVideoPtsUs > kForwardSeekThresholdUs;
    if (needSeek && !seekVideoStream(sourceUs))
        return false;

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    if (!packet || !frame) {
        av_frame_free(&frame);
        av_packet_free(&packet);
        return false;
    }

    const AVRational timeBase = m_fmt->streams[m_videoStream]->time_base;
    QImage best;
    drift::TimeUs bestDelta = INT64_MAX;
    bool found = false;
    bool done = false;

    while (!done && av_read_frame(m_fmt, packet) >= 0) {
        if (packet->stream_index != m_videoStream) {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(m_videoCtx, packet) < 0) {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);

        while (true) {
            const int rc = avcodec_receive_frame(m_videoCtx, frame);
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
                break;
            if (rc < 0) {
                done = true;
                break;
            }

            const drift::TimeUs ptsUs = ptsToUs(frame, timeBase);
            m_lastVideoPtsUs = ptsUs;
            const drift::TimeUs delta = qAbs(ptsUs - sourceUs);
            if (delta < bestDelta) {
                QImage image;
                if (convertFrame(frame, image, targetWidth, targetHeight)) {
                    bestDelta = delta;
                    best = image;
                    found = true;
                }
            }

            if (ptsUs >= sourceUs) {
                done = true;
                break;
            }
        }
    }

    av_frame_free(&frame);
    av_packet_free(&packet);

    if (found) {
        out = best;
        m_lastVideoFrame = best;
        m_lastVideoW = targetWidth;
        m_lastVideoH = targetHeight;
        m_videoPositioned = true;
    } else {
        // Hit EOF or a decode error; force a reseek on the next request.
        m_videoPositioned = false;
    }
    return found;
}

int ClipReader::readAudioInterleaved(drift::TimeUs sourceStartUs, int sampleCount, int outputSampleRate,
                                     float *interleavedStereoOut)
{
    if (!interleavedStereoOut || sampleCount <= 0 || outputSampleRate <= 0)
        return 0;
    if (!ensureAudioDecoder())
        return 0;

    // Re-seek only on a real discontinuity. During normal playback the request
    // advances by exactly one buffer, so we keep decoding forward from where we
    // left off — no per-buffer seek, no resampler reset, no glitching.
    const bool rateChanged = m_outputSampleRate != outputSampleRate;
    m_outputSampleRate = outputSampleRate;
    const bool needSeek = rateChanged || !m_audioPositioned
                          || sourceStartUs < m_audioNextPtsUs - kAudioSeekToleranceUs
                          || sourceStartUs > m_audioNextPtsUs + kAudioForwardSeekThresholdUs;

    bool alignToStart = false;
    if (needSeek) {
        if (!seekAudioStream(sourceStartUs)) // flushes the codec and frees m_swr
            return 0;
        m_audioLeftover.clear();
        m_audioPositioned = true;
        alignToStart = true;
    }

    if (!m_swr) {
        AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
        if (swr_alloc_set_opts2(&m_swr, &outLayout, AV_SAMPLE_FMT_FLT, outputSampleRate,
                                &m_audioCtx->ch_layout, static_cast<AVSampleFormat>(m_audioCtx->sample_fmt),
                                m_audioCtx->sample_rate, 0, nullptr)
                < 0
            || swr_init(m_swr) < 0) {
            if (m_swr)
                swr_free(&m_swr);
            return 0;
        }
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    if (!packet || !frame) {
        av_frame_free(&frame);
        av_packet_free(&packet);
        return 0;
    }

    const AVRational timeBase = m_fmt->streams[m_audioStream]->time_base;
    QVector<float> scratch;
    int pendingDrop = 0; // leading output frames to discard so playback starts at sourceStartUs
    bool sentFlush = false;

    while (m_audioLeftover.size() < sampleCount * 2) {
        const int rc = avcodec_receive_frame(m_audioCtx, frame);
        if (rc == AVERROR(EAGAIN)) {
            if (sentFlush)
                break;
            if (av_read_frame(m_fmt, packet) < 0) {
                avcodec_send_packet(m_audioCtx, nullptr); // drain the decoder at EOF
                sentFlush = true;
                continue;
            }
            if (packet->stream_index != m_audioStream) {
                av_packet_unref(packet);
                continue;
            }
            avcodec_send_packet(m_audioCtx, packet);
            av_packet_unref(packet);
            continue;
        }
        if (rc < 0) // AVERROR_EOF or a decode error
            break;

        if (alignToStart) {
            const drift::TimeUs framePtsUs = ptsToUs(frame, timeBase);
            m_audioNextPtsUs = framePtsUs;
            if (sourceStartUs > framePtsUs)
                pendingDrop = static_cast<int>(((sourceStartUs - framePtsUs) * outputSampleRate)
                                               / drift::kUsPerSecond);
            alignToStart = false;
        }

        const int maxOut = swr_get_out_samples(m_swr, frame->nb_samples);
        scratch.resize(maxOut * 2);
        uint8_t *outData[1] = {reinterpret_cast<uint8_t *>(scratch.data())};
        const int converted = swr_convert(m_swr, outData, maxOut,
                                          const_cast<const uint8_t **>(frame->data), frame->nb_samples);
        if (converted <= 0)
            continue;

        int offset = 0;
        if (pendingDrop > 0) {
            const int drop = qMin(pendingDrop, converted);
            offset = drop;
            pendingDrop -= drop;
            m_audioNextPtsUs += static_cast<drift::TimeUs>(drop) * drift::kUsPerSecond / outputSampleRate;
        }
        for (int i = offset * 2; i < converted * 2; ++i)
            m_audioLeftover.append(scratch[i]);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);

    const int outFrames = qMin(sampleCount, static_cast<int>(m_audioLeftover.size() / 2));
    if (outFrames > 0) {
        std::memcpy(interleavedStereoOut, m_audioLeftover.constData(),
                    static_cast<size_t>(outFrames) * 2 * sizeof(float));
        m_audioLeftover.remove(0, outFrames * 2);
        m_audioNextPtsUs += static_cast<drift::TimeUs>(outFrames) * drift::kUsPerSecond / outputSampleRate;
    }
    return outFrames;
}
