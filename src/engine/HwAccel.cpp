#include "HwAccel.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavutil/log.h>
}

namespace drift::hwaccel {

QList<Backend> decodeBackendOrder()
{
#if defined(Q_OS_MACOS)
    return {Backend::VideoToolbox};
#elif defined(Q_OS_WIN)
    return {Backend::Cuda, Backend::D3d11va};
#else
    return {Backend::Cuda, Backend::Vaapi};
#endif
}

QList<Backend> availableDecodeBackends()
{
    // Honour the kill switch here, not just at decode time: this is what probes the
    // devices, and a wedged driver would otherwise hang the picker on startup — the
    // exact case someone sets DRIFT_NO_HWACCEL to get out of.
    if (disabledByEnv())
        return {};

    QList<Backend> out;
    for (const Backend backend : decodeBackendOrder()) {
        if (deviceAvailable(deviceType(backend)))
            out.append(backend);
    }
    return out;
}

AVHWDeviceType deviceType(Backend backend)
{
    switch (backend) {
    case Backend::Cuda:
        return AV_HWDEVICE_TYPE_CUDA;
    case Backend::D3d11va:
        return AV_HWDEVICE_TYPE_D3D11VA;
    case Backend::Vaapi:
        return AV_HWDEVICE_TYPE_VAAPI;
    case Backend::VideoToolbox:
        return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
    case Backend::None:
        break;
    }
    return AV_HWDEVICE_TYPE_NONE;
}

const char *name(Backend backend)
{
    switch (backend) {
    case Backend::Cuda:
        return "NVDEC";
    case Backend::D3d11va:
        return "Direct3D 11";
    case Backend::Vaapi:
        return "VAAPI";
    case Backend::VideoToolbox:
        return "VideoToolbox";
    case Backend::None:
        break;
    }
    return "";
}

QString id(Backend backend)
{
    switch (backend) {
    case Backend::Cuda:
        return QStringLiteral("nvdec");
    case Backend::D3d11va:
        return QStringLiteral("d3d11va");
    case Backend::Vaapi:
        return QStringLiteral("vaapi");
    case Backend::VideoToolbox:
        return QStringLiteral("videotoolbox");
    case Backend::None:
        break;
    }
    return {};
}

Backend backendFromId(const QString &id)
{
    for (const Backend backend :
         {Backend::Cuda, Backend::D3d11va, Backend::Vaapi, Backend::VideoToolbox}) {
        if (drift::hwaccel::id(backend) == id)
            return backend;
    }
    return Backend::None;
}

const char *scaleFilter(Backend backend)
{
    switch (backend) {
    case Backend::Cuda:
        return "scale_cuda";
    case Backend::Vaapi:
        return "scale_vaapi";
    case Backend::VideoToolbox:
        return "scale_vt";
    case Backend::D3d11va:
    case Backend::None:
        break;
    }
    return nullptr;
}

bool deviceAvailable(AVHWDeviceType type)
{
    if (type == AV_HWDEVICE_TYPE_NONE)
        return false;
    static QMutex mutex;
    static QHash<int, bool> cache;
    QMutexLocker lock(&mutex);
    const auto it = cache.constFind(static_cast<int>(type));
    if (it != cache.cend())
        return it.value();

    AVBufferRef *ctx = nullptr;
    const int previousLog = av_log_get_level();
    av_log_set_level(AV_LOG_QUIET);
    const int err = av_hwdevice_ctx_create(&ctx, type, nullptr, nullptr, 0);
    av_log_set_level(previousLog);
    if (ctx)
        av_buffer_unref(&ctx);
    const bool ok = err >= 0;
    cache.insert(static_cast<int>(type), ok);
    return ok;
}

bool disabledByEnv()
{
    return qEnvironmentVariableIsSet("DRIFT_NO_HWACCEL");
}

const AVCodec *findDecoder(AVCodecID codecId, AVHWDeviceType type, AVPixelFormat *pixFmt)
{
    if (type == AV_HWDEVICE_TYPE_NONE)
        return nullptr;
    void *iter = nullptr;
    while (const AVCodec *codec = av_codec_iterate(&iter)) {
        if (!av_codec_is_decoder(codec) || codec->id != codecId)
            continue;
        for (int i = 0;; ++i) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
            if (!config)
                break;
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                && config->device_type == type) {
                if (pixFmt)
                    *pixFmt = config->pix_fmt;
                return codec;
            }
        }
    }
    return nullptr;
}

} // namespace drift::hwaccel
