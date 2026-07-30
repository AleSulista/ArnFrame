#include "MediaWaveform.h"

#include <QFileInfo>
#include <QVector>
#include <QtMath>

#include <cmath>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/samplefmt.h>
}

namespace {

float frameSampleAbs(const AVFrame *frame, int channels, int channel, int sample)
{
    switch (frame->format) {
    case AV_SAMPLE_FMT_FLTP:
        return qAbs(reinterpret_cast<const float *>(frame->data[channel])[sample]);
    case AV_SAMPLE_FMT_FLT:
        return qAbs(reinterpret_cast<const float *>(frame->data[0])[sample * channels + channel]);
    case AV_SAMPLE_FMT_S16P:
        return qAbs(reinterpret_cast<const int16_t *>(frame->data[channel])[sample]) / 32768.0f;
    case AV_SAMPLE_FMT_S16:
        return qAbs(reinterpret_cast<const int16_t *>(frame->data[0])[sample * channels + channel])
            / 32768.0f;
    case AV_SAMPLE_FMT_S32P:
        return qAbs(reinterpret_cast<const int32_t *>(frame->data[channel])[sample])
            / 2147483648.0f;
    case AV_SAMPLE_FMT_S32:
        return qAbs(reinterpret_cast<const int32_t *>(frame->data[0])[sample * channels + channel])
            / 2147483648.0f;
    case AV_SAMPLE_FMT_DBLP:
        return static_cast<float>(
            qAbs(reinterpret_cast<const double *>(frame->data[channel])[sample]));
    case AV_SAMPLE_FMT_DBL:
        return static_cast<float>(
            qAbs(reinterpret_cast<const double *>(frame->data[0])[sample * channels + channel]));
    default:
        return 0.0f;
    }
}

int64_t estimateDurationSamples(const AVFormatContext *fmt, const AVStream *stream, int sampleRate)
{
    if (sampleRate <= 0)
        return 0;

    if (stream->duration > 0 && stream->time_base.num > 0 && stream->time_base.den > 0) {
        return av_rescale_q(stream->duration, stream->time_base, AVRational{1, sampleRate});
    }

    // fmt->duration is in AV_TIME_BASE (µs), not samples.
    if (fmt->duration > 0)
        return av_rescale(fmt->duration, sampleRate, AV_TIME_BASE);

    return 0;
}

void ingestFramePeaks(const AVFrame *frame, int channels, int sampleCount, int64_t durationSamples,
                      int64_t &totalSamples, QVector<float> &buckets)
{
    const int samples = frame->nb_samples;
    if (samples <= 0 || channels <= 0)
        return;

    for (int s = 0; s < samples; ++s) {
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c)
            peak = qMax(peak, frameSampleAbs(frame, channels, c, s));

        const int64_t denom = durationSamples > 0 ? durationSamples : qMax<int64_t>(totalSamples + 1, 1);
        const int idx = qBound(0, static_cast<int>((totalSamples * sampleCount) / denom),
                              sampleCount - 1);
        buckets[idx] = qMax(buckets[idx], peak);
        ++totalSamples;
    }
}

QVector<float> downsamplePeaks(const QVector<float> &fine, int sampleCount)
{
    if (fine.isEmpty() || sampleCount <= 0)
        return {};
    if (fine.size() <= sampleCount)
        return fine;

    QVector<float> buckets(sampleCount, 0.0f);
    for (int i = 0; i < fine.size(); ++i) {
        const int idx = qBound(0, static_cast<int>((static_cast<int64_t>(i) * sampleCount) / fine.size()),
                              sampleCount - 1);
        buckets[idx] = qMax(buckets[idx], fine[i]);
    }
    return buckets;
}

} // namespace

MediaWaveform::Dense MediaWaveform::densePeaks(const QString &sourcePath, int peaksPerSecond,
                                               int maxPeaks)
{
    Dense result;
    if (peaksPerSecond <= 0 || maxPeaks <= 0)
        return result;

    const QString absolutePath = QFileInfo(sourcePath).absoluteFilePath();
    if (absolutePath.isEmpty())
        return result;

    AVFormatContext *fmt = nullptr;
    if (avformat_open_input(&fmt, absolutePath.toUtf8().constData(), nullptr, nullptr) < 0)
        return result;
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return result;
    }

    int audioStreamIndex = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (audioStreamIndex < 0) {
        avformat_close_input(&fmt);
        return result;
    }

    const AVStream *stream = fmt->streams[audioStreamIndex];
    const AVCodecParameters *codecPar = stream->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        avformat_close_input(&fmt);
        return result;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecPar);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmt);
        return result;
    }

    const int sampleRate = codecCtx->sample_rate > 0 ? codecCtx->sample_rate : codecPar->sample_rate;
    int64_t durationSamples = estimateDurationSamples(fmt, stream, sampleRate);

    const double knownDuration = (sampleRate > 0 && durationSamples > 0)
        ? static_cast<double>(durationSamples) / sampleRate
        : 0.0;
    // Fallback when container duration is missing: ~200 peaks/sec, then downsample.
    const bool useFine = knownDuration <= 0.0;
    const int sampleCount = useFine
        ? 0
        : qBound<int>(1, static_cast<int>(std::ceil(knownDuration * peaksPerSecond)), maxPeaks);

    QVector<float> buckets(sampleCount, 0.0f);
    QVector<float> finePeaks;
    const int fineHop = qMax(1, sampleRate > 0 ? sampleRate / 200 : 1);
    float fineMax = 0.0f;
    int fineCount = 0;

    int64_t totalSamples = 0;

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    auto processFrame = [&](AVFrame *decoded) {
        const int samples = decoded->nb_samples;
        const int channels = codecCtx->ch_layout.nb_channels;
        if (samples <= 0 || channels <= 0)
            return;
        if (av_get_bytes_per_sample(static_cast<AVSampleFormat>(decoded->format)) <= 0)
            return;

        if (useFine) {
            for (int s = 0; s < samples; ++s) {
                float peak = 0.0f;
                for (int c = 0; c < channels; ++c)
                    peak = qMax(peak, frameSampleAbs(decoded, channels, c, s));
                fineMax = qMax(fineMax, peak);
                if (++fineCount >= fineHop) {
                    finePeaks.append(fineMax);
                    fineMax = 0.0f;
                    fineCount = 0;
                }
                ++totalSamples;
            }
        } else {
            ingestFramePeaks(decoded, channels, sampleCount, durationSamples, totalSamples, buckets);
        }
    };

    while (packet && frame && av_read_frame(fmt, packet) >= 0) {
        if (packet->stream_index != audioStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(codecCtx, packet) < 0) {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);

        while (true) {
            const int rc = avcodec_receive_frame(codecCtx, frame);
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
                break;
            if (rc < 0)
                break;
            processFrame(frame);
        }
    }

    // Drain decoder.
    if (packet && frame) {
        avcodec_send_packet(codecCtx, nullptr);
        while (true) {
            const int rc = avcodec_receive_frame(codecCtx, frame);
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
                break;
            if (rc < 0)
                break;
            processFrame(frame);
        }
    }

    if (useFine && fineCount > 0)
        finePeaks.append(fineMax);

    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmt);

    if (totalSamples == 0) {
        result.peaks = QVector<float>(sampleCount, 0.15f);
        result.durationSeconds = knownDuration;
        return result;
    }

    if (useFine) {
        // Duration is whatever we actually decoded, at fineHop samples per fine peak.
        const double decodedSeconds = sampleRate > 0
            ? static_cast<double>(totalSamples) / sampleRate
            : 0.0;
        const int target = qBound<int>(
            1, static_cast<int>(std::ceil(decodedSeconds * peaksPerSecond)), maxPeaks);
        result.peaks = downsamplePeaks(finePeaks, target);
        result.durationSeconds = decodedSeconds;
        return result;
    }

    result.durationSeconds = knownDuration;

    // If the container overstated duration, peaks only filled a prefix — spread
    // them across the full bucket range using the samples we actually decoded, which
    // also makes the decoded length the span the buckets now cover.
    if (durationSamples > totalSamples + totalSamples / 10) {
        QVector<float> fixed(sampleCount, 0.0f);
        for (int b = 0; b < sampleCount; ++b) {
            if (buckets[b] <= 0.0f)
                continue;
            const int b2 = qBound(
                0,
                static_cast<int>((static_cast<int64_t>(b) * durationSamples) / totalSamples),
                sampleCount - 1);
            fixed[b2] = qMax(fixed[b2], buckets[b]);
        }
        buckets = fixed;
        if (sampleRate > 0)
            result.durationSeconds = static_cast<double>(totalSamples) / sampleRate;
    }

    result.peaks = buckets;
    return result;
}

QVariantList MediaWaveform::voicePeaksFromPcm(const float *interleavedStereo, int frameCount,
                                              int sampleRate, int buckets)
{
    QVariantList result;
    if (!interleavedStereo || frameCount <= 0 || buckets <= 0 || sampleRate <= 0)
        return result;

    // One-pole band-pass coefficients for the speech band.
    const double dt = 1.0 / static_cast<double>(sampleRate);
    const double rcLow = 1.0 / (2.0 * M_PI * 3500.0);  // low-pass @ 3.5 kHz
    const double rcHigh = 1.0 / (2.0 * M_PI * 150.0);  // high-pass @ 150 Hz
    const double lpAlpha = dt / (rcLow + dt);
    const double hpAlpha = rcHigh / (rcHigh + dt);

    QVector<float> peaks(buckets, 0.0f);

    double lpPrev = 0.0;   // low-pass state
    double hpPrev = 0.0;   // high-pass output state
    double hpXPrev = 0.0;  // high-pass previous input (post low-pass)
    float maxPeak = 0.0f;

    for (int i = 0; i < frameCount; ++i) {
        const double mono = 0.5 * (static_cast<double>(interleavedStereo[i * 2])
                                   + static_cast<double>(interleavedStereo[i * 2 + 1]));
        lpPrev += lpAlpha * (mono - lpPrev);
        const double hp = hpAlpha * (hpPrev + lpPrev - hpXPrev);
        hpPrev = hp;
        hpXPrev = lpPrev;

        const int bucket = qBound(0, static_cast<int>((static_cast<int64_t>(i) * buckets) / frameCount),
                                  buckets - 1);
        const float amp = static_cast<float>(qAbs(hp));
        if (amp > peaks[bucket])
            peaks[bucket] = amp;
        maxPeak = qMax(maxPeak, amp);
    }

    // Normalize to the loudest voice moment so speech fills the lane while
    // silence/music sit near the floor.
    const double norm = maxPeak > 1e-6f ? 1.0 / static_cast<double>(maxPeak) : 0.0;
    for (int i = 0; i < buckets; ++i)
        result.append(qBound(0.05, static_cast<double>(peaks[i]) * norm, 1.0));

    return result;
}
