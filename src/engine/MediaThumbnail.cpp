#include "MediaThumbnail.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QStandardPaths>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace {

QString cachePathFor(const QString &sourcePath)
{
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             + QStringLiteral("/thumbnails");
    QDir().mkpath(cacheDir);

    const QString key = QString::number(qHash(QFileInfo(sourcePath).absoluteFilePath()));
    return cacheDir + QLatin1Char('/') + key + QStringLiteral(".jpg");
}

bool saveFrameAsJpeg(const AVFrame *frame, const QString &outPath)
{
    SwsContext *sws = sws_getContext(frame->width, frame->height,
                                     static_cast<AVPixelFormat>(frame->format),
                                     320, 180, AV_PIX_FMT_RGB24, SWS_BILINEAR,
                                     nullptr, nullptr, nullptr);
    if (!sws)
        return false;

    AVFrame *rgb = av_frame_alloc();
    if (!rgb) {
        sws_freeContext(sws);
        return false;
    }

    rgb->format = AV_PIX_FMT_RGB24;
    rgb->width = 320;
    rgb->height = 180;
    if (av_frame_get_buffer(rgb, 0) < 0) {
        av_frame_free(&rgb);
        sws_freeContext(sws);
        return false;
    }

    sws_scale(sws, frame->data, frame->linesize, 0, frame->height, rgb->data, rgb->linesize);

    QImage image(rgb->data[0], 320, 180, rgb->linesize[0], QImage::Format_RGB888);
    const bool ok = image.copy().save(outPath, "JPG", 85);

    av_frame_free(&rgb);
    sws_freeContext(sws);
    return ok;
}

bool decodeVideoThumbnail(AVFormatContext *fmt, int videoStreamIndex, AVCodecContext *codecCtx,
                          const QString &outPath)
{
    AVStream *stream = fmt->streams[videoStreamIndex];

    const int64_t seekTarget = stream->duration > 0 ? stream->duration / 10 : 0;
    if (seekTarget > 0)
        av_seek_frame(fmt, videoStreamIndex, seekTarget, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecCtx);

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    bool saved = false;
    int packetsRead = 0;

    while (!saved && packetsRead < 200 && av_read_frame(fmt, packet) >= 0) {
        ++packetsRead;
        if (packet->stream_index != videoStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(codecCtx, packet) < 0) {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);

        while (!saved) {
            const int rc = avcodec_receive_frame(codecCtx, frame);
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF)
                break;
            if (rc < 0)
                return false;

            saved = saveFrameAsJpeg(frame, outPath);
            if (saved)
                break;
        }
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    return saved;
}

} // namespace

QString MediaThumbnail::generate(const QString &sourcePath, const QString &kind)
{
    const QString absolutePath = QFileInfo(sourcePath).absoluteFilePath();
    if (absolutePath.isEmpty() || !QFile::exists(absolutePath))
        return {};

    const QString outPath = cachePathFor(absolutePath);
    if (QFile::exists(outPath))
        return outPath;

    if (kind == QStringLiteral("image")) {
        QImageReader reader(absolutePath);
        reader.setAutoTransform(true);
        QImage image = reader.read();
        if (image.isNull())
            return {};
        image = image.scaled(320, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (!image.save(outPath, "JPG", 85))
            return {};
        return outPath;
    }

    AVFormatContext *fmt = nullptr;
    if (avformat_open_input(&fmt, absolutePath.toUtf8().constData(), nullptr, nullptr) < 0)
        return {};

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return {};
    }

    int videoStreamIndex = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (videoStreamIndex < 0) {
        avformat_close_input(&fmt);
        return {};
    }

    const AVCodecParameters *codecPar = fmt->streams[videoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        avformat_close_input(&fmt);
        return {};
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecPar);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmt);
        return {};
    }

    const bool saved = decodeVideoThumbnail(fmt, videoStreamIndex, codecCtx, outPath);

    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmt);

    return saved ? outPath : QString();
}
