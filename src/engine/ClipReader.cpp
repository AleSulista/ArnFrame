#include "ClipReader.h"

#include <QThread>
#include <QtMath>

#include <cmath>
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

bool isHardwarePixelFormat(AVPixelFormat fmt)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(fmt);
    return desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL);
}

// Prefer the VAAPI surface format when the decoder offers it; otherwise pick the
// first software format so get_format never hard-fails with AV_PIX_FMT_NONE
// (that path leaves the hwaccel decoder in a half-initialized state).
AVPixelFormat hwGetFormat(AVCodecContext *ctx, const AVPixelFormat *pixFmts)
{
    const AVPixelFormat prefer =
        ctx && ctx->opaque ? *static_cast<const AVPixelFormat *>(ctx->opaque) : AV_PIX_FMT_NONE;

    for (const AVPixelFormat *p = pixFmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == prefer)
            return *p;
    }

    for (const AVPixelFormat *p = pixFmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (!isHardwarePixelFormat(*p))
            return *p;
    }

    return pixFmts ? pixFmts[0] : AV_PIX_FMT_NONE;
}

QImage frameToRgba(const AVFrame *frame, SwsContext *&sws, int targetWidth, int targetHeight)
{
    if (!frame || targetWidth <= 0 || targetHeight <= 0)
        return {};
    if (isHardwarePixelFormat(static_cast<AVPixelFormat>(frame->format)))
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

void ClipReader::teardownVideoDecoder()
{
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    if (m_videoCtx)
        avcodec_free_context(&m_videoCtx);
    if (m_hwDeviceCtx)
        av_buffer_unref(&m_hwDeviceCtx);
    m_hwAccelActive = false;
    m_hwPixFmt = AV_PIX_FMT_NONE;
    m_videoPositioned = false;
    m_lastVideoPtsUs = 0;
    m_decodeW = 0;
    m_decodeH = 0;
    m_videoCache.clear();
}

QSize ClipReader::decodeSizeFor(int maxWidth, int maxHeight) const
{
    const AVCodecParameters *par = m_fmt->streams[m_videoStream]->codecpar;
    const int srcW = par->width;
    const int srcH = par->height;
    if (srcW <= 0 || srcH <= 0)
        return {qMax(1, maxWidth), qMax(1, maxHeight)};
    if (maxWidth <= 0 || maxHeight <= 0)
        return {srcW, srcH};

    // Never decode larger than the source; scaling up is the compositor's job.
    const double fit = qMin(static_cast<double>(maxWidth) / srcW, static_cast<double>(maxHeight) / srcH);
    if (fit >= 1.0)
        return {srcW, srcH};

    // Quantize up to 1/8 steps. A preview panel dragged a few pixels wider must
    // not change the decode size, or every resize would drop the frame cache.
    const double quantized = qMin(1.0, std::ceil(fit * 8.0) / 8.0);
    const int w = qMax(2, static_cast<int>(std::lround(srcW * quantized)) & ~1);
    const int h = qMax(2, static_cast<int>(std::lround(srcH * quantized)) & ~1);
    return {w, h};
}

void ClipReader::applyDecodeSize(const QSize &size)
{
    if (m_decodeW == size.width() && m_decodeH == size.height())
        return;

    // A new decode size invalidates the cached images (they are the wrong size)
    // but NOT the demux position — there is no reason to seek.
    m_decodeW = size.width();
    m_decodeH = size.height();
    m_videoCache.clear();
}

drift::TimeUs ClipReader::frameToleranceUs() const
{
    // Half a source frame: the nearest-frame window. The old fixed 40 ms was
    // longer than a frame above ~25 fps, so it returned stale frames.
    if (m_sourceFrameDurationUs > 0)
        return qMax<drift::TimeUs>(1, m_sourceFrameDurationUs / 2);
    return 20'000;
}

bool ClipReader::lookupCachedFrame(drift::TimeUs sourceUs, QImage &out) const
{
    const drift::TimeUs tolerance = frameToleranceUs();
    drift::TimeUs bestDelta = tolerance + 1;
    int bestIndex = -1;
    for (int i = 0; i < m_videoCache.size(); ++i) {
        const drift::TimeUs delta = qAbs(m_videoCache.at(i).ptsUs - sourceUs);
        if (delta <= tolerance && delta < bestDelta) {
            bestDelta = delta;
            bestIndex = i;
        }
    }
    if (bestIndex < 0)
        return false;

    out = m_videoCache.at(bestIndex).image;
    return true;
}

void ClipReader::storeCachedFrame(drift::TimeUs ptsUs, const QImage &image)
{
    if (image.isNull())
        return;

    for (int i = 0; i < m_videoCache.size(); ++i) {
        if (m_videoCache.at(i).ptsUs == ptsUs) {
            m_videoCache.move(i, 0);
            return;
        }
    }

    m_videoCache.prepend(CachedFrame{ptsUs, image});
    while (m_videoCache.size() > kMaxCachedFrames)
        m_videoCache.removeLast();
}

void ClipReader::close()
{
    if (m_swr)
        swr_free(&m_swr);

    teardownVideoDecoder();

    if (m_audioCtx)
        avcodec_free_context(&m_audioCtx);
    if (m_fmt)
        avformat_close_input(&m_fmt);

    m_videoStream = -1;
    m_audioStream = -1;
    m_hwAccelDisabled = false;
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

    if (m_videoStream >= 0) {
        const AVRational rate = m_fmt->streams[m_videoStream]->avg_frame_rate;
        if (rate.num > 0 && rate.den > 0) {
            m_sourceFrameDurationUs =
                static_cast<drift::TimeUs>(std::llround(drift::kUsPerSecond * double(rate.den) / rate.num));
        }
    }

    return hasVideo() || hasAudio();
}

bool ClipReader::openSoftwareVideoDecoder()
{
    if (!m_fmt || m_videoStream < 0)
        return false;

    const AVCodecParameters *par = m_fmt->streams[m_videoStream]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec)
        return false;

    m_videoCtx = avcodec_alloc_context3(codec);
    if (!m_videoCtx)
        return false;

    if (avcodec_parameters_to_context(m_videoCtx, par) < 0) {
        avcodec_free_context(&m_videoCtx);
        return false;
    }

    // Left at defaults this decodes single-threaded on most builds. Each reader
    // already owns a thread, so keep the fan-out modest rather than per-core.
    m_videoCtx->thread_count = qBound(1, QThread::idealThreadCount() / 2, 4);
    m_videoCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

    if (avcodec_open2(m_videoCtx, codec, nullptr) < 0) {
        avcodec_free_context(&m_videoCtx);
        return false;
    }

    m_hwAccelActive = false;
    m_hwPixFmt = AV_PIX_FMT_NONE;
    return true;
}

bool ClipReader::tryOpenHardwareDecoder()
{
    if (!m_fmt || m_videoStream < 0 || m_hwAccelActive || m_hwAccelDisabled)
        return m_hwAccelActive;

    // Allow forcing software decode on broken VAAPI stacks.
    if (qEnvironmentVariableIsSet("DRIFT_NO_VAAPI")) {
        m_hwAccelDisabled = true;
        return false;
    }

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
        m_hwAccelDisabled = true;
        return false;
    }

    m_videoCtx = avcodec_alloc_context3(codec);
    if (!m_videoCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwPixFmt = AV_PIX_FMT_NONE;
        return false;
    }

    if (avcodec_parameters_to_context(m_videoCtx, par) < 0) {
        avcodec_free_context(&m_videoCtx);
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwPixFmt = AV_PIX_FMT_NONE;
        return false;
    }

    m_videoCtx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
    m_videoCtx->opaque = &m_hwPixFmt;
    m_videoCtx->get_format = hwGetFormat;

    if (avcodec_open2(m_videoCtx, codec, nullptr) < 0) {
        avcodec_free_context(&m_videoCtx);
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwPixFmt = AV_PIX_FMT_NONE;
        m_hwAccelDisabled = true;
        return false;
    }

    m_hwAccelActive = true;
    return true;
}

bool ClipReader::fallbackFromHardwareDecoder()
{
    if (!m_hwAccelActive && !m_hwDeviceCtx)
        return openSoftwareVideoDecoder();

    teardownVideoDecoder();
    m_hwAccelDisabled = true;
    return openSoftwareVideoDecoder();
}

bool ClipReader::ensureVideoDecoder()
{
    if (!m_fmt || m_videoStream < 0)
        return false;
    if (m_videoCtx)
        return true;

    if (tryOpenHardwareDecoder())
        return true;

    return openSoftwareVideoDecoder();
}

bool ClipReader::transferHwFrameToImage(const AVFrame *hwFrame, QImage &out, int targetWidth, int targetHeight)
{
    AVFrame *swFrame = av_frame_alloc();
    if (!swFrame)
        return false;

    // Transfer into a fresh software frame. On failure the destination may be
    // partially populated — always free it before returning.
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
    if (!frame)
        return false;

    if (m_hwAccelActive && frame->format == m_hwPixFmt)
        return transferHwFrameToImage(frame, out, targetWidth, targetHeight);

    // If get_format fell back to software while hw_device_ctx is still set,
    // treat the frame as a normal software frame.
    if (isHardwarePixelFormat(static_cast<AVPixelFormat>(frame->format)))
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
    if (!m_audioCtx)
        return false;
    if (avcodec_parameters_to_context(m_audioCtx, par) < 0) {
        avcodec_free_context(&m_audioCtx);
        return false;
    }
    if (avcodec_open2(m_audioCtx, codec, nullptr) < 0) {
        avcodec_free_context(&m_audioCtx);
        return false;
    }
    return true;
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

bool ClipReader::decodeVideoFrameAtOnce(drift::TimeUs sourceUs, QImage &out, int maxWidth, int maxHeight,
                                        bool *hwFailure)
{
    if (hwFailure)
        *hwFailure = false;
    if (!ensureVideoDecoder())
        return false;

    applyDecodeSize(decodeSizeFor(maxWidth, maxHeight));

    if (lookupCachedFrame(sourceUs, out))
        return true;

    const drift::TimeUs tolerance = frameToleranceUs();
    const bool needSeek = !m_videoPositioned || sourceUs < m_lastVideoPtsUs - tolerance
                          || sourceUs - m_lastVideoPtsUs > kForwardSeekThresholdUs;
    if (needSeek && !seekVideoStream(sourceUs))
        return false;

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *best = av_frame_alloc();
    if (!packet || !frame || !best) {
        av_frame_free(&best);
        av_frame_free(&frame);
        av_packet_free(&packet);
        return false;
    }

    const AVRational timeBase = m_fmt->streams[m_videoStream]->time_base;
    drift::TimeUs bestDelta = INT64_MAX;
    drift::TimeUs bestPtsUs = 0;
    bool found = false;
    bool done = false;
    bool sawHwFailure = false;

    auto markHwFailure = [&]() {
        if (m_hwAccelActive) {
            sawHwFailure = true;
            done = true;
        }
    };

    while (!done && av_read_frame(m_fmt, packet) >= 0) {
        if (packet->stream_index != m_videoStream) {
            av_packet_unref(packet);
            continue;
        }

        int sendRc = avcodec_send_packet(m_videoCtx, packet);
        av_packet_unref(packet);
        if (sendRc == AVERROR(EAGAIN)) {
            // Decoder is full; drain below then retry is handled by the next read.
            // Fall through to receive.
        } else if (sendRc < 0) {
            markHwFailure();
            continue;
        }

        while (!done) {
            const int rc = avcodec_receive_frame(m_videoCtx, frame);
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
                break;
            if (rc < 0) {
                // VAAPI often fails here with "hardware accelerator failed to
                // decode picture". The frame may be partially initialized —
                // unref before any further use or free.
                av_frame_unref(frame);
                markHwFailure();
                break;
            }

            const drift::TimeUs ptsUs = ptsToUs(frame, timeBase);
            m_lastVideoPtsUs = ptsUs;
            const drift::TimeUs delta = qAbs(ptsUs - sourceUs);
            // Keep a reference to the best frame and convert only once, after the
            // loop. Converting every frame between the keyframe and the target was
            // an sws_scale + full copy per frame of the GOP, all but one discarded.
            if (delta < bestDelta) {
                bestDelta = delta;
                bestPtsUs = ptsUs;
                av_frame_unref(best);
                if (av_frame_ref(best, frame) < 0) {
                    av_frame_unref(frame);
                    done = true;
                    break;
                }
                found = true;
            }
            av_frame_unref(frame);

            if (ptsUs >= sourceUs) {
                done = true;
                break;
            }
        }
    }

    QImage converted;
    bool convertedOk = false;
    if (found && !sawHwFailure) {
        convertedOk = convertFrame(best, converted, m_decodeW, m_decodeH);
        if (!convertedOk && m_hwAccelActive
            && (best->format == m_hwPixFmt
                || isHardwarePixelFormat(static_cast<AVPixelFormat>(best->format)))) {
            // Transfer from the VAAPI surface failed — abandon hwaccel.
            sawHwFailure = true;
        }
    }

    av_frame_unref(best);
    av_frame_free(&best);
    av_frame_unref(frame);
    av_frame_free(&frame);
    av_packet_free(&packet);

    if (sawHwFailure) {
        if (hwFailure)
            *hwFailure = true;
        m_videoPositioned = false;
        return false;
    }

    if (convertedOk) {
        out = converted;
        storeCachedFrame(bestPtsUs, converted);
        m_videoPositioned = true;
        return true;
    }

    m_videoPositioned = false;
    return false;
}

bool ClipReader::readVideoFrameAt(drift::TimeUs sourceUs, QImage &out, int maxWidth, int maxHeight)
{
    bool hwFailure = false;
    if (decodeVideoFrameAtOnce(sourceUs, out, maxWidth, maxHeight, &hwFailure))
        return true;

    if (!hwFailure)
        return false;

    // Sticky software fallback for this reader — continuing with a broken VAAPI
    // context is what triggers free(): invalid size on subsequent frames.
    if (!fallbackFromHardwareDecoder())
        return false;

    return decodeVideoFrameAtOnce(sourceUs, out, maxWidth, maxHeight, nullptr);
}

void ClipReader::prefetchNextVideoFrame(int maxWidth, int maxHeight)
{
    if (!m_videoPositioned || m_sourceFrameDurationUs <= 0)
        return;

    // Decode the frame after the current position into the cache, so the next
    // request hits while the caller is busy compositing this one.
    QImage ignored;
    readVideoFrameAt(m_lastVideoPtsUs + m_sourceFrameDurationUs, ignored, maxWidth, maxHeight);
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
        if (rc < 0) { // AVERROR_EOF or a decode error
            av_frame_unref(frame);
            break;
        }

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

    av_frame_unref(frame);
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
