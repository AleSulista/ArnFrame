#include "Exporter.h"

#include "AudioMixer.h"
#include "FrameCompositor.h"
#include "core/Project.h"
#include "core/Time.h"

#include <QFile>
#include <QImage>

#include <cmath>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace {

constexpr int kAudioBitrate = 192000;

// Sends a frame (or nullptr to flush) to the encoder and interleaves the packets.
bool encodeWriteFrame(AVFormatContext *fmt, AVCodecContext *codec, AVStream *stream, AVFrame *frame,
                      AVPacket *pkt, QString *errorOut)
{
    int rc = avcodec_send_frame(codec, frame);
    if (rc < 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Encoder rejected a frame");
        return false;
    }

    while (rc >= 0) {
        rc = avcodec_receive_packet(codec, pkt);
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
            break;
        if (rc < 0) {
            if (errorOut)
                *errorOut = QStringLiteral("Failed to read an encoded packet");
            return false;
        }
        av_packet_rescale_ts(pkt, codec->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        rc = av_interleaved_write_frame(fmt, pkt);
        av_packet_unref(pkt);
        if (rc < 0) {
            if (errorOut)
                *errorOut = QStringLiteral("Failed to write a packet");
            return false;
        }
    }
    return true;
}

// Copies one audio frame's worth of interleaved stereo floats into a planar frame.
void fillPlanarAudio(AVFrame *frame, const float *interleaved, int samples)
{
    auto *left = reinterpret_cast<float *>(frame->data[0]);
    auto *right = reinterpret_cast<float *>(frame->data[1]);
    for (int i = 0; i < samples; ++i) {
        left[i] = interleaved[i * 2];
        right[i] = interleaved[i * 2 + 1];
    }
}

} // namespace

const QList<ExportPreset> &Exporter::presets()
{
    static const QList<ExportPreset> kPresets = {
        {QStringLiteral("source"), QStringLiteral("Same as project"), 0, 0, 16000},
        {QStringLiteral("1080p"), QStringLiteral("1080p"), 1080, 0, 12000},
        {QStringLiteral("720p"), QStringLiteral("720p"), 720, 0, 8000},
        {QStringLiteral("480p"), QStringLiteral("480p"), 480, 0, 4000},
    };
    return kPresets;
}

const ExportPreset *Exporter::presetById(const QString &id)
{
    for (const ExportPreset &preset : presets()) {
        if (preset.id == id)
            return &preset;
    }
    return nullptr;
}

bool Exporter::run(const drift::Project &project, const ExportPreset &preset, const QString &outputPath,
                   QString *errorOut, const ProgressFn &onProgress)
{
    const int projW = project.width();
    const int projH = project.height();
    if (projW <= 0 || projH <= 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Invalid project resolution");
        return false;
    }

    int outH = preset.targetHeight > 0 ? preset.targetHeight : projH;
    int outW = static_cast<int>(std::llround(static_cast<double>(projW) * outH / projH));
    outW &= ~1; // H.264 needs even dimensions
    outH &= ~1;
    outW = qMax(2, outW);
    outH = qMax(2, outH);

    const int fps = preset.fps > 0 ? preset.fps : qMax(1, project.fps());
    const int sampleRate = project.sampleRate() > 0 ? project.sampleRate() : 48000;
    const drift::TimeUs durationUs = project.durationUs();
    if (durationUs <= 0) {
        if (errorOut)
            *errorOut = QStringLiteral("Timeline is empty");
        return false;
    }

    const int64_t totalFrames = qMax<int64_t>(1, std::llround(static_cast<double>(durationUs) * fps / 1e6));
    const int64_t totalAudioSamples =
        qMax<int64_t>(0, std::llround(static_cast<double>(durationUs) * sampleRate / 1e6));

    // All libav resources declared up front so a single cleanup path can free them.
    AVFormatContext *fmt = nullptr;
    AVCodecContext *vctx = nullptr;
    AVCodecContext *actx = nullptr;
    AVStream *vstream = nullptr;
    AVStream *astream = nullptr;
    AVFrame *vframe = nullptr;
    AVFrame *aframe = nullptr;
    AVPacket *pkt = nullptr;
    SwsContext *sws = nullptr;
    bool ok = false;
    bool cancelled = false;
    bool headerWritten = false;
    QString error;

    // Encode to a sibling temp file and atomically rename on success, so the
    // destination path never holds a half-written (unplayable, no-moov) file.
    const QString tmpPath = outputPath + QStringLiteral(".part");
    const QByteArray outUtf8 = outputPath.toUtf8();
    const QByteArray tmpUtf8 = tmpPath.toUtf8();
    if (QFile::exists(tmpPath))
        QFile::remove(tmpPath);

    // Guess the muxer from the real output extension, but write to the temp path.
    avformat_alloc_output_context2(&fmt, nullptr, nullptr, outUtf8.constData());
    if (!fmt) {
        error = QStringLiteral("Could not determine output format");
        goto cleanup;
    }

    {
        const AVCodec *vcodec = avcodec_find_encoder(AV_CODEC_ID_H264);
        const AVCodec *acodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!vcodec || !acodec) {
            error = QStringLiteral("H.264/AAC encoder not available");
            goto cleanup;
        }

        vstream = avformat_new_stream(fmt, nullptr);
        astream = avformat_new_stream(fmt, nullptr);
        if (!vstream || !astream) {
            error = QStringLiteral("Could not create output streams");
            goto cleanup;
        }

        vctx = avcodec_alloc_context3(vcodec);
        actx = avcodec_alloc_context3(acodec);
        if (!vctx || !actx) {
            error = QStringLiteral("Could not allocate encoders");
            goto cleanup;
        }

        vctx->width = outW;
        vctx->height = outH;
        vctx->pix_fmt = AV_PIX_FMT_YUV420P;
        vctx->time_base = AVRational{1, fps};
        vctx->framerate = AVRational{fps, 1};
        vctx->bit_rate = static_cast<int64_t>(preset.videoBitrateKbps) * 1000;
        vctx->gop_size = fps * 2;
        vctx->max_b_frames = 2;
        if (fmt->oformat->flags & AVFMT_GLOBALHEADER)
            vctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        av_opt_set(vctx->priv_data, "preset", "medium", 0);

        actx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        actx->sample_rate = sampleRate;
        av_channel_layout_default(&actx->ch_layout, 2);
        actx->bit_rate = kAudioBitrate;
        actx->time_base = AVRational{1, sampleRate};
        if (fmt->oformat->flags & AVFMT_GLOBALHEADER)
            actx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(vctx, vcodec, nullptr) < 0) {
            error = QStringLiteral("Could not open the video encoder");
            goto cleanup;
        }
        if (avcodec_open2(actx, acodec, nullptr) < 0) {
            error = QStringLiteral("Could not open the audio encoder");
            goto cleanup;
        }

        avcodec_parameters_from_context(vstream->codecpar, vctx);
        avcodec_parameters_from_context(astream->codecpar, actx);
        vstream->time_base = vctx->time_base;
        astream->time_base = actx->time_base;

        const int frameSize = actx->frame_size > 0 ? actx->frame_size : 1024;

        if (!(fmt->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&fmt->pb, tmpUtf8.constData(), AVIO_FLAG_WRITE) < 0) {
                error = QStringLiteral("Could not open the output file");
                goto cleanup;
            }
        }

        if (avformat_write_header(fmt, nullptr) < 0) {
            error = QStringLiteral("Could not write the file header");
            goto cleanup;
        }
        headerWritten = true;

        sws = sws_getContext(projW, projH, AV_PIX_FMT_RGBA, outW, outH, AV_PIX_FMT_YUV420P, SWS_BILINEAR,
                             nullptr, nullptr, nullptr);
        if (!sws) {
            error = QStringLiteral("Could not create the scaler");
            goto cleanup;
        }

        pkt = av_packet_alloc();
        vframe = av_frame_alloc();
        aframe = av_frame_alloc();
        if (!pkt || !vframe || !aframe) {
            error = QStringLiteral("Out of memory");
            goto cleanup;
        }

        vframe->format = AV_PIX_FMT_YUV420P;
        vframe->width = outW;
        vframe->height = outH;
        if (av_frame_get_buffer(vframe, 32) < 0) {
            error = QStringLiteral("Could not allocate the video frame");
            goto cleanup;
        }

        aframe->format = AV_SAMPLE_FMT_FLTP;
        aframe->sample_rate = sampleRate;
        av_channel_layout_copy(&aframe->ch_layout, &actx->ch_layout);
        aframe->nb_samples = frameSize;
        if (av_frame_get_buffer(aframe, 0) < 0) {
            error = QStringLiteral("Could not allocate the audio frame");
            goto cleanup;
        }

        FrameCompositor compositor;
        compositor.setProject(&project);
        AudioMixer mixer;
        mixer.setProject(&project);

        std::vector<float> audioBuffer; // pending interleaved stereo
        int64_t audioSamplesGenerated = 0;
        int64_t audioPts = 0;

        auto flushAudioFrames = [&](bool drainAll) -> bool {
            while (static_cast<int64_t>(audioBuffer.size()) >= static_cast<int64_t>(frameSize) * 2) {
                if (av_frame_make_writable(aframe) < 0) {
                    error = QStringLiteral("Audio frame not writable");
                    return false;
                }
                aframe->nb_samples = frameSize;
                fillPlanarAudio(aframe, audioBuffer.data(), frameSize);
                aframe->pts = audioPts;
                audioPts += frameSize;
                if (!encodeWriteFrame(fmt, actx, astream, aframe, pkt, &error))
                    return false;
                audioBuffer.erase(audioBuffer.begin(), audioBuffer.begin() + frameSize * 2);
            }
            if (drainAll && !audioBuffer.empty()) {
                const int remaining = static_cast<int>(audioBuffer.size() / 2);
                if (av_frame_make_writable(aframe) < 0) {
                    error = QStringLiteral("Audio frame not writable");
                    return false;
                }
                aframe->nb_samples = remaining;
                fillPlanarAudio(aframe, audioBuffer.data(), remaining);
                aframe->pts = audioPts;
                audioPts += remaining;
                if (!encodeWriteFrame(fmt, actx, astream, aframe, pkt, &error))
                    return false;
                audioBuffer.clear();
            }
            return true;
        };

        for (int64_t i = 0; i < totalFrames; ++i) {
            if (onProgress && !onProgress(static_cast<double>(i) / totalFrames)) {
                cancelled = true;
                break;
            }

            const drift::TimeUs t = static_cast<drift::TimeUs>(std::llround(static_cast<double>(i) * 1e6 / fps));
            QImage img = compositor.compositeAt(t);
            if (img.isNull()) {
                img = QImage(projW, projH, QImage::Format_RGBA8888);
                img.fill(Qt::black);
            } else if (img.format() != QImage::Format_RGBA8888) {
                img = img.convertToFormat(QImage::Format_RGBA8888);
            }
            if (img.width() != projW || img.height() != projH)
                img = img.scaled(projW, projH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            if (av_frame_make_writable(vframe) < 0) {
                error = QStringLiteral("Video frame not writable");
                goto cleanup;
            }
            {
                const uint8_t *srcData[4] = {img.constBits(), nullptr, nullptr, nullptr};
                const int srcStride[4] = {static_cast<int>(img.bytesPerLine()), 0, 0, 0};
                sws_scale(sws, srcData, srcStride, 0, projH, vframe->data, vframe->linesize);
            }
            vframe->pts = i;
            if (!encodeWriteFrame(fmt, vctx, vstream, vframe, pkt, &error))
                goto cleanup;

            // Generate audio through the end of this video frame, keeping A/V aligned.
            const int64_t targetSamples =
                qMin(totalAudioSamples, ((i + 1) * static_cast<int64_t>(sampleRate)) / fps);
            const int need = static_cast<int>(targetSamples - audioSamplesGenerated);
            if (need > 0) {
                const size_t base = audioBuffer.size();
                audioBuffer.resize(base + static_cast<size_t>(need) * 2);
                const drift::TimeUs audioStartUs = static_cast<drift::TimeUs>(
                    (static_cast<int64_t>(audioSamplesGenerated) * drift::kUsPerSecond) / sampleRate);
                mixer.mix(audioStartUs, need, sampleRate, audioBuffer.data() + base);
                audioSamplesGenerated = targetSamples;
            }
            if (!flushAudioFrames(false))
                goto cleanup;
        }

        if (!cancelled) {
            if (!flushAudioFrames(true))
                goto cleanup;
            if (!encodeWriteFrame(fmt, vctx, vstream, nullptr, pkt, &error))
                goto cleanup;
            if (!encodeWriteFrame(fmt, actx, astream, nullptr, pkt, &error))
                goto cleanup;
            if (av_write_trailer(fmt) < 0) {
                error = QStringLiteral("Could not finalize the file");
                goto cleanup;
            }
            ok = true;
        }
    }

cleanup:
    if (sws)
        sws_freeContext(sws);
    if (vframe)
        av_frame_free(&vframe);
    if (aframe)
        av_frame_free(&aframe);
    if (pkt)
        av_packet_free(&pkt);
    if (vctx)
        avcodec_free_context(&vctx);
    if (actx)
        avcodec_free_context(&actx);
    if (fmt) {
        if (headerWritten && !ok && fmt->pb) {
            // Best-effort: finalize so the container isn't left half-written before removal.
            av_write_trailer(fmt);
        }
        if (fmt->pb && !(fmt->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmt->pb);
        avformat_free_context(fmt);
    }

    if (ok) {
        // Atomically publish the finished file.
        if (QFile::exists(outputPath))
            QFile::remove(outputPath);
        if (!QFile::rename(tmpPath, outputPath)) {
            QFile::remove(tmpPath);
            ok = false;
            error = QStringLiteral("Could not finalize the output file");
        }
    } else if (QFile::exists(tmpPath)) {
        QFile::remove(tmpPath);
    }

    if (!ok && errorOut) {
        *errorOut = cancelled ? QStringLiteral("Export cancelled")
                              : (error.isEmpty() ? QStringLiteral("Export failed") : error);
    }
    return ok;
}
