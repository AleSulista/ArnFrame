#include "MediaThumbnail.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QStandardPaths>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace {

QString cacheDir()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + QStringLiteral("/thumbnails");
    QDir().mkpath(dir);
    return dir;
}

QString cacheKeyFor(const QString &sourcePath)
{
    return QString::number(qHash(QFileInfo(sourcePath).absoluteFilePath()));
}

QString cachePathFor(const QString &sourcePath)
{
    return cacheDir() + QLatin1Char('/') + cacheKeyFor(sourcePath) + QStringLiteral(".jpg");
}

QString cacheStripPathFor(const QString &sourcePath)
{
    return cacheDir() + QLatin1Char('/') + cacheKeyFor(sourcePath) + QStringLiteral("_strip.jpg");
}

bool isValidCacheFile(const QString &path)
{
    return QFile::exists(path) && QFileInfo(path).size() > 128;
}

QImage frameToImage(const AVFrame *frame, int width, int height)
{
    SwsContext *sws = sws_getContext(frame->width, frame->height,
                                     static_cast<AVPixelFormat>(frame->format),
                                     width, height, AV_PIX_FMT_RGB24, SWS_BILINEAR,
                                     nullptr, nullptr, nullptr);
    if (!sws)
        return {};

    AVFrame *rgb = av_frame_alloc();
    if (!rgb) {
        sws_freeContext(sws);
        return {};
    }

    rgb->format = AV_PIX_FMT_RGB24;
    rgb->width = width;
    rgb->height = height;
    if (av_frame_get_buffer(rgb, 0) < 0) {
        av_frame_free(&rgb);
        sws_freeContext(sws);
        return {};
    }

    sws_scale(sws, frame->data, frame->linesize, 0, frame->height, rgb->data, rgb->linesize);

    QImage image(rgb->data[0], width, height, rgb->linesize[0], QImage::Format_RGB888);
    const QImage copy = image.copy();

    av_frame_free(&rgb);
    sws_freeContext(sws);
    return copy;
}

bool decodeNextVideoFrame(AVFormatContext *fmt, int videoStreamIndex, AVCodecContext *codecCtx,
                          AVPacket *packet, AVFrame *frame, QImage &outImage, int width, int height)
{
    while (av_read_frame(fmt, packet) >= 0) {
        if (packet->stream_index != videoStreamIndex) {
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
                return false;

            outImage = frameToImage(frame, width, height);
            return !outImage.isNull();
        }
    }

    return false;
}

bool seekAndDecodeFrame(AVFormatContext *fmt, int videoStreamIndex, AVCodecContext *codecCtx,
                        int64_t timeUs, QImage &outImage, int width, int height)
{
    AVStream *stream = fmt->streams[videoStreamIndex];
    const int64_t targetTs = av_rescale_q(timeUs, {1, AV_TIME_BASE}, stream->time_base);
    av_seek_frame(fmt, videoStreamIndex, targetTs, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecCtx);

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    const bool ok = packet && frame && decodeNextVideoFrame(fmt, videoStreamIndex, codecCtx,
                                                            packet, frame, outImage, width, height);

    av_frame_free(&frame);
    av_packet_free(&packet);
    return ok;
}

bool decodeFirstVideoFrame(AVFormatContext *fmt, int videoStreamIndex, AVCodecContext *codecCtx,
                           const QString &outPath, int width, int height)
{
    avcodec_flush_buffers(codecCtx);

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    QImage image;
    bool saved = false;
    int packetsRead = 0;

    while (!saved && packetsRead < 400 && packet && frame) {
        if (!decodeNextVideoFrame(fmt, videoStreamIndex, codecCtx, packet, frame, image, width, height))
            break;
        ++packetsRead;
        saved = image.save(outPath, "JPG", 85);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    return saved;
}

bool openVideoDecoder(const QString &absolutePath, AVFormatContext **fmtOut,
                      int *videoStreamIndexOut, AVCodecContext **codecCtxOut)
{
    AVFormatContext *fmt = nullptr;
    if (avformat_open_input(&fmt, absolutePath.toUtf8().constData(), nullptr, nullptr) < 0)
        return false;

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt);
        return false;
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
        return false;
    }

    const AVCodecParameters *codecPar = fmt->streams[videoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        avformat_close_input(&fmt);
        return false;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecPar);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmt);
        return false;
    }

    *fmtOut = fmt;
    *videoStreamIndexOut = videoStreamIndex;
    *codecCtxOut = codecCtx;
    return true;
}

} // namespace

QString MediaThumbnail::generate(const QString &sourcePath, const QString &kind)
{
    const QString absolutePath = QFileInfo(sourcePath).absoluteFilePath();
    if (absolutePath.isEmpty() || !QFile::exists(absolutePath))
        return {};

    const QString outPath = cachePathFor(absolutePath);
    if (isValidCacheFile(outPath))
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

    if (kind != QStringLiteral("video"))
        return {};

    AVFormatContext *fmt = nullptr;
    int videoStreamIndex = -1;
    AVCodecContext *codecCtx = nullptr;
    if (!openVideoDecoder(absolutePath, &fmt, &videoStreamIndex, &codecCtx))
        return {};

    const bool saved = decodeFirstVideoFrame(fmt, videoStreamIndex, codecCtx, outPath, 320, 180);

    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmt);

    return saved ? outPath : QString();
}

QString MediaThumbnail::generateFilmstrip(const QString &sourcePath, const QString &kind)
{
    const QString absolutePath = QFileInfo(sourcePath).absoluteFilePath();
    if (absolutePath.isEmpty() || !QFile::exists(absolutePath))
        return {};

    if (kind == QStringLiteral("image"))
        return generate(absolutePath, kind);

    if (kind != QStringLiteral("video"))
        return {};

    const QString outPath = cacheStripPathFor(absolutePath);
    if (isValidCacheFile(outPath))
        return outPath;

    AVFormatContext *fmt = nullptr;
    int videoStreamIndex = -1;
    AVCodecContext *codecCtx = nullptr;
    if (!openVideoDecoder(absolutePath, &fmt, &videoStreamIndex, &codecCtx))
        return {};

    const int64_t durationUs = fmt->duration != AV_NOPTS_VALUE ? fmt->duration : 0;
    const int frameW = kFilmstripFrameWidth;
    const int frameH = kFilmstripFrameHeight;
    const int frameCount = kFilmstripFrameCount;

    QImage strip(frameW * frameCount, frameH, QImage::Format_RGB888);
    strip.fill(Qt::black);

    bool anyFrame = false;
    for (int i = 0; i < frameCount; ++i) {
        const int64_t timeUs = durationUs > 0 ? (durationUs * i) / frameCount : 0;
        QImage frame;
        if (!seekAndDecodeFrame(fmt, videoStreamIndex, codecCtx, timeUs, frame, frameW, frameH))
            continue;

        anyFrame = true;
        QPainter painter(&strip);
        painter.drawImage(i * frameW, 0, frame);
    }

    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmt);

    if (!anyFrame || !strip.save(outPath, "JPG", 85))
        return {};

    return outPath;
}
