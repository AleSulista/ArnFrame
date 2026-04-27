#include "MediaWaveform.h"

#include <QFileInfo>
#include <QVector>
#include <QtMath>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

QVariantList MediaWaveform::peaks(const QString &sourcePath, int sampleCount)
{
    QVariantList result;
    if (sampleCount <= 0)
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

    const AVCodecParameters *codecPar = fmt->streams[audioStreamIndex]->codecpar;
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

    QVector<float> buckets(sampleCount, 0.0f);
    QVector<int> bucketCounts(sampleCount, 0);
    int64_t totalSamples = 0;

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

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

            const int samples = frame->nb_samples;
            const int channels = codecCtx->ch_layout.nb_channels;
            if (samples <= 0 || channels <= 0)
                continue;

            if (frame->format == AV_SAMPLE_FMT_FLTP) {
                for (int s = 0; s < samples; ++s) {
                    float peak = 0.0f;
                    for (int c = 0; c < channels; ++c) {
                        const float *data = reinterpret_cast<const float *>(frame->data[c]);
                        peak = qMax(peak, qAbs(data[s]));
                    }
                    const int bucket = static_cast<int>((totalSamples * sampleCount)
                                                        / qMax<int64_t>(fmt->duration > 0 ? fmt->duration : samples, 1));
                    const int idx = qBound(0, bucket, sampleCount - 1);
                    buckets[idx] = qMax(buckets[idx], peak);
                    ++bucketCounts[idx];
                    ++totalSamples;
                }
            }
        }
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmt);

    if (totalSamples == 0) {
        for (int i = 0; i < sampleCount; ++i)
            result.append(0.15);
        return result;
    }

    for (int i = 0; i < sampleCount; ++i)
        result.append(qBound(0.05, static_cast<double>(buckets[i]), 1.0));

    return result;
}
