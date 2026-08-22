#include "DebugReport.h"

#include "ClipReader.h"
#include "Exporter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QRegularExpression>
#include <QSet>
#include <QSurfaceFormat>
#include <QSysInfo>
#include <QThread>
#include <QVariantList>
#include <utility>

#ifndef DRIFT_VERSION
#define DRIFT_VERSION "0.0.0"
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
}

namespace {

QString trReport(const char *text)
{
    return QCoreApplication::translate("DebugReport", text);
}

QString readKeyValueFile(const QString &path, const QString &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.startsWith('#') || !line.contains('='))
            continue;
        const int eq = line.indexOf('=');
        if (QString::fromUtf8(line.left(eq)) != key)
            continue;
        QByteArray value = line.mid(eq + 1);
        if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"'))
            value = value.mid(1, value.size() - 2);
        return QString::fromUtf8(value);
    }
    return {};
}

QString readTrimmedFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll().trimmed());
}

quint32 readSysfsHex(const QString &path)
{
    QString text = readTrimmedFile(path);
    if (text.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        text = text.mid(2);
    bool ok = false;
    const quint32 value = text.toUInt(&ok, 16);
    return ok ? value : 0;
}

QString osReleasePretty(const QString &path)
{
    const QString pretty = readKeyValueFile(path, QStringLiteral("PRETTY_NAME"));
    if (!pretty.isEmpty())
        return pretty;
    const QString name = readKeyValueFile(path, QStringLiteral("NAME"));
    const QString version = readKeyValueFile(path, QStringLiteral("VERSION_ID"));
    if (name.isEmpty())
        return {};
    return version.isEmpty() ? name : QStringLiteral("%1 %2").arg(name, version);
}

QString cpuModel()
{
#if defined(Q_OS_LINUX)
    QFile cpu(QStringLiteral("/proc/cpuinfo"));
    if (cpu.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!cpu.atEnd()) {
            const QByteArray line = cpu.readLine();
            if (!line.startsWith("model name"))
                continue;
            const int colon = line.indexOf(':');
            if (colon < 0)
                continue;
            return QString::fromUtf8(line.mid(colon + 1).trimmed());
        }
    }
#endif
    return QSysInfo::currentCpuArchitecture();
}

QString packageKind()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macOS");
#else
    if (qEnvironmentVariableIsSet("FLATPAK_ID") || QFile::exists(QStringLiteral("/.flatpak-info")))
        return QStringLiteral("Flatpak");
    if (qEnvironmentVariableIsSet("APPIMAGE"))
        return QStringLiteral("AppImage");
    if (qEnvironmentVariableIsSet("SNAP"))
        return QStringLiteral("Snap");
    return QStringLiteral("Linux");
#endif
}

// Flatpak bind-mounts the host file at /run/host/os-release (no extra
// filesystem permission). /run/host/etc/os-release is systemd-nspawn's
// layout. /etc/os-release inside the sandbox is the KDE/Freedesktop runtime.
QString hostOsReleasePath()
{
    static const QString kCandidates[] = {
        QStringLiteral("/run/host/os-release"),
        QStringLiteral("/run/host/etc/os-release"),
        QStringLiteral("/etc/os-release"),
    };
    for (const QString &path : kCandidates) {
        if (QFile::exists(path))
            return path;
    }
    return {};
}

QString osPretty()
{
#if defined(Q_OS_LINUX)
    if (const QString pretty = osReleasePretty(hostOsReleasePath()); !pretty.isEmpty())
        return pretty;
#endif
    return QSysInfo::prettyProductName();
}

QString pciVendorName(quint16 id)
{
    switch (id) {
    case 0x8086:
        return QStringLiteral("Intel");
    case 0x10de:
        return QStringLiteral("NVIDIA");
    case 0x1002:
    case 0x1022:
        return QStringLiteral("AMD");
    case 0x14e4:
        return QStringLiteral("Broadcom");
    case 0x1af4:
        return QStringLiteral("Virtio");
    case 0x15ad:
        return QStringLiteral("VMware");
    case 0x1234:
    case 0x1b36:
        return QStringLiteral("QEMU");
    case 0x13b5:
        return QStringLiteral("ARM");
    case 0x106b:
        return QStringLiteral("Apple");
    case 0x17cb:
        return QStringLiteral("Qualcomm");
    case 0x1414:
        return QStringLiteral("Microsoft");
    default:
        return {};
    }
}

QString driverNameAt(const QString &deviceDir)
{
    const QFileInfo link(deviceDir + QStringLiteral("/driver"));
    if (!link.exists())
        return {};
    return QFileInfo(link.symLinkTarget()).fileName();
}

QString nvidiaModelForSlot(const QString &slot)
{
    QFile file(QStringLiteral("/proc/driver/nvidia/gpus/%1/information").arg(slot));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        if (!line.startsWith("Model:"))
            continue;
        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;
        return QString::fromUtf8(line.mid(colon + 1).trimmed());
    }
    return {};
}

struct GpuAdapter {
    QString slot;
    QString vendor;
    quint16 vendorId = 0;
    quint16 deviceId = 0;
    QString driver;
    QString model;
};

QString pciIdString(const GpuAdapter &gpu)
{
    if (gpu.vendorId == 0 && gpu.deviceId == 0)
        return {};
    return QStringLiteral("%1:%2")
        .arg(gpu.vendorId, 4, 16, QLatin1Char('0'))
        .arg(gpu.deviceId, 4, 16, QLatin1Char('0'))
        .toUpper();
}

QString formatGpu(const GpuAdapter &gpu)
{
    QString head = gpu.model;
    if (head.isEmpty() && !gpu.vendor.isEmpty())
        head = QStringLiteral("%1 Graphics").arg(gpu.vendor);
    if (head.isEmpty())
        head = trReport("Unknown GPU");

    QStringList bits;
    if (!gpu.driver.isEmpty())
        bits.append(gpu.driver);
    if (const QString pci = pciIdString(gpu); !pci.isEmpty())
        bits.append(pci);
    if (bits.isEmpty())
        return head;
    return QStringLiteral("%1 (%2)").arg(head, bits.join(QStringLiteral(", ")));
}

GpuAdapter gpuFromSysfsDevice(const QString &deviceDir, const QString &slot)
{
    GpuAdapter gpu;
    gpu.slot = slot;
    gpu.vendorId = static_cast<quint16>(readSysfsHex(deviceDir + QStringLiteral("/vendor")));
    gpu.deviceId = static_cast<quint16>(readSysfsHex(deviceDir + QStringLiteral("/device")));
    gpu.vendor = pciVendorName(gpu.vendorId);
    if (gpu.vendor.isEmpty() && gpu.vendorId)
        gpu.vendor = QStringLiteral("PCI %1").arg(gpu.vendorId, 4, 16, QLatin1Char('0')).toUpper();
    gpu.driver = driverNameAt(deviceDir);
    if (const QString label = readTrimmedFile(deviceDir + QStringLiteral("/label")); !label.isEmpty())
        gpu.model = label;
    else if (const QString product = readTrimmedFile(deviceDir + QStringLiteral("/product_name"));
             !product.isEmpty())
        gpu.model = product;
    if (gpu.model.isEmpty() && !slot.isEmpty())
        gpu.model = nvidiaModelForSlot(slot);
    return gpu;
}

QList<GpuAdapter> enumerateGpus()
{
    QList<GpuAdapter> gpus;
    QSet<QString> seen;

    const auto addGpu = [&](GpuAdapter gpu) {
        const QString key = !gpu.slot.isEmpty() ? gpu.slot : pciIdString(gpu);
        if (key.isEmpty() || seen.contains(key))
            return;
        if (gpu.vendorId == 0 && gpu.driver.isEmpty())
            return;
        seen.insert(key);
        gpus.append(std::move(gpu));
    };

#if defined(Q_OS_LINUX)
    const QDir drmDir(QStringLiteral("/sys/class/drm"));
    const QRegularExpression cardRe(QStringLiteral("^card\\d+$"));
    const QFileInfoList cards = drmDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &card : cards) {
        if (!cardRe.match(card.fileName()).hasMatch())
            continue;
        const QString deviceDir = card.absoluteFilePath() + QStringLiteral("/device");
        QString slot = QFileInfo(QFileInfo(deviceDir).canonicalFilePath()).fileName();
        if (!slot.contains(QLatin1Char(':')))
            slot.clear();
        addGpu(gpuFromSysfsDevice(deviceDir, slot));
    }

    const QDir pciDir(QStringLiteral("/sys/bus/pci/devices"));
    const QFileInfoList devices = pciDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &device : devices) {
        const quint32 pciClass = readSysfsHex(device.absoluteFilePath() + QStringLiteral("/class"));
        if ((pciClass >> 16) != 0x03)
            continue;
        addGpu(gpuFromSysfsDevice(device.absoluteFilePath(), device.fileName()));
    }
#endif
    return gpus;
}

QString openglRenderer()
{
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return {};

    QOffscreenSurface surface;
    surface.setFormat(QSurfaceFormat::defaultFormat());
    surface.create();
    if (!surface.isValid())
        return {};

    QOpenGLContext ctx;
    ctx.setFormat(surface.format());
    if (!ctx.create() || !ctx.makeCurrent(&surface))
        return {};

    QString renderer;
    if (QOpenGLFunctions *fn = ctx.functions()) {
        const char *glRenderer = reinterpret_cast<const char *>(fn->glGetString(GL_RENDERER));
        const char *glVendor = reinterpret_cast<const char *>(fn->glGetString(GL_VENDOR));
        if (glRenderer)
            renderer = QString::fromUtf8(glRenderer);
        if (glVendor) {
            const QString vendor = QString::fromUtf8(glVendor);
            if (!vendor.isEmpty() && !renderer.contains(vendor, Qt::CaseInsensitive))
                renderer = renderer.isEmpty() ? vendor : QStringLiteral("%1 (%2)").arg(renderer, vendor);
        }
    }
    ctx.doneCurrent();
    return renderer;
}

bool decoderHasVaapi(const AVCodec *codec)
{
    if (!codec)
        return false;
    for (int i = 0;; ++i) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
        if (!config)
            return false;
        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
            && config->device_type == AV_HWDEVICE_TYPE_VAAPI)
            return true;
    }
}

const AVCodec *findVaapiDecoder(AVCodecID codecId)
{
    void *iter = nullptr;
    while (const AVCodec *codec = av_codec_iterate(&iter)) {
        if (av_codec_is_decoder(codec) && codec->id == codecId && decoderHasVaapi(codec))
            return codec;
    }
    return nullptr;
}

bool vaapiDeviceAvailable()
{
#if defined(Q_OS_LINUX)
    AVBufferRef *ctx = nullptr;
    const int err = av_hwdevice_ctx_create(&ctx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
    if (ctx)
        av_buffer_unref(&ctx);
    return err >= 0;
#else
    return false;
#endif
}

const AVCodec *findNamedEncoder(const char *const *names)
{
    for (int i = 0; names && names[i]; ++i) {
        if (const AVCodec *codec = avcodec_find_encoder_by_name(names[i]))
            return codec;
    }
    return nullptr;
}

QString decodeModeLabel()
{
    switch (ClipReader::hardwareDecodeMode()) {
    case ClipReader::HardwareDecodeMode::Software:
        return trReport("Software");
    case ClipReader::HardwareDecodeMode::Hardware:
        return trReport("Hardware");
    case ClipReader::HardwareDecodeMode::Auto:
        break;
    }
    return trReport("Auto");
}

QVariantMap systemRow(const QString &label, const QString &value)
{
    return {{QStringLiteral("label"), label}, {QStringLiteral("value"), value}};
}

QString supportLabel(bool ok)
{
    return ok ? trReport("Supported") : trReport("Not supported");
}

} // namespace

QVariantMap DebugReport::collect()
{
    QVariantMap info;
    const QString package = packageKind();
    const bool vaapiOk = vaapiDeviceAvailable();

    struct CodecSpec {
        const char *name;
        AVCodecID id;
    };
    static const CodecSpec kCodecs[] = {
        {"H264", AV_CODEC_ID_H264},
        {"VP9", AV_CODEC_ID_VP9},
        {"VP8", AV_CODEC_ID_VP8},
        {"AV1", AV_CODEC_ID_AV1},
        {"HEVC", AV_CODEC_ID_HEVC},
    };

    QVariantList codecs;
    for (const CodecSpec &spec : kCodecs) {
        const AVCodec *software = avcodec_find_decoder(spec.id);
        const AVCodec *hardware = findVaapiDecoder(spec.id);
        QVariantMap row;
        row.insert(QStringLiteral("name"), QString::fromLatin1(spec.name));
        row.insert(QStringLiteral("software"), software != nullptr);
        row.insert(QStringLiteral("hardware"), vaapiOk && hardware != nullptr);
        row.insert(QStringLiteral("softwareDecoder"),
                   software ? QString::fromUtf8(software->name) : QString());
        row.insert(QStringLiteral("hardwareDecoder"),
                   hardware ? QString::fromUtf8(hardware->name) : QString());
        codecs.append(row);
    }

    // Software names match Exporter's catalog. Hardware is the first available
    // NVENC/QSV/AMF/VAAPI/VideoToolbox encoder for that family, if any.
    static const char *const kH264Enc[] = {"libx264", "h264", nullptr};
    static const char *const kVp9Enc[] = {"libvpx-vp9", nullptr};
    static const char *const kVp8Enc[] = {"libvpx", nullptr};
    static const char *const kAv1Enc[] = {"libsvtav1", nullptr};
    static const char *const kHevcEnc[] = {"libx265", "hevc", nullptr};
    struct EncoderSpec {
        const char *name;
        const char *const *softwareNames;
        const char *hwIdPrefix;
    };
    static const EncoderSpec kEncoders[] = {
        {"H264", kH264Enc, "h264"},
        {"VP9", kVp9Enc, "vp9"},
        {"VP8", kVp8Enc, "vp8"},
        {"AV1", kAv1Enc, "av1"},
        {"HEVC", kHevcEnc, "h265"},
    };

    const QVariantList exportCodecs = Exporter::videoCodecs();
    auto firstHwEncoder = [&exportCodecs](const char *prefix) -> QVariantMap {
        const QString pre = QString::fromLatin1(prefix);
        for (const QVariant &v : exportCodecs) {
            const QVariantMap m = v.toMap();
            if (!m.value(QStringLiteral("hardware")).toBool())
                continue;
            if (!m.value(QStringLiteral("id")).toString().startsWith(pre))
                continue;
            if (m.value(QStringLiteral("available")).toBool())
                return m;
        }
        return {};
    };

    QVariantList encoders;
    for (const EncoderSpec &spec : kEncoders) {
        const AVCodec *software = findNamedEncoder(spec.softwareNames);
        const QVariantMap hw = firstHwEncoder(spec.hwIdPrefix);
        QVariantMap row;
        row.insert(QStringLiteral("name"), QString::fromLatin1(spec.name));
        row.insert(QStringLiteral("software"), software != nullptr);
        row.insert(QStringLiteral("hardware"), !hw.isEmpty());
        row.insert(QStringLiteral("softwareEncoder"),
                   software ? QString::fromUtf8(software->name) : QString());
        row.insert(QStringLiteral("hardwareEncoder"),
                   hw.value(QStringLiteral("encoderName")).toString());
        encoders.append(row);
    }

    QVariantList system;
    system.append(systemRow(trReport("Drift"), QStringLiteral(DRIFT_VERSION)));
    system.append(systemRow(trReport("Package"), package));
    if (const QString flatpakId = qEnvironmentVariable("FLATPAK_ID"); !flatpakId.isEmpty())
        system.append(systemRow(trReport("Flatpak ID"), flatpakId));
    if (const QString runtime = readKeyValueFile(QStringLiteral("/.flatpak-info"), QStringLiteral("runtime"));
        !runtime.isEmpty())
        system.append(systemRow(trReport("Flatpak runtime"), runtime));
    if (const QString appImage = qEnvironmentVariable("APPIMAGE"); !appImage.isEmpty())
        system.append(systemRow(trReport("AppImage"), appImage));
    system.append(systemRow(trReport("OS"), osPretty()));
#if defined(Q_OS_LINUX)
    {
        const QString versionId = readKeyValueFile(hostOsReleasePath(), QStringLiteral("VERSION_ID"));
        if (!versionId.isEmpty())
            system.append(systemRow(trReport("Distro version"), versionId));
    }
#endif
    system.append(systemRow(trReport("Kernel"),
                            QStringLiteral("%1 %2").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion())));
    system.append(systemRow(trReport("Architecture"), QSysInfo::currentCpuArchitecture()));
    system.append(systemRow(trReport("CPU"), cpuModel()));
    system.append(systemRow(trReport("CPU threads"), QString::number(QThread::idealThreadCount())));

    const QList<GpuAdapter> gpus = enumerateGpus();
    if (gpus.isEmpty()) {
        system.append(systemRow(trReport("GPU"), trReport("Unknown")));
    } else if (gpus.size() == 1) {
        system.append(systemRow(trReport("GPU"), formatGpu(gpus.first())));
    } else {
        for (int i = 0; i < gpus.size(); ++i) {
            system.append(systemRow(trReport("GPU %1").arg(i + 1), formatGpu(gpus.at(i))));
        }
    }
    if (const QString gl = openglRenderer(); !gl.isEmpty())
        system.append(systemRow(QStringLiteral("OpenGL"), gl));

    system.append(systemRow(trReport("Qt"), QString::fromLatin1(qVersion())));
    system.append(systemRow(trReport("FFmpeg"), QString::fromUtf8(av_version_info())));
    system.append(systemRow(trReport("VAAPI"),
                            vaapiOk ? trReport("Available") : trReport("Not available")));
    system.append(systemRow(trReport("Preview decode"), decodeModeLabel()));
    system.append(systemRow(trReport("Locale"), QLocale::system().name()));
    if (qEnvironmentVariableIsSet("DRIFT_NO_VAAPI"))
        system.append(systemRow(QStringLiteral("DRIFT_NO_VAAPI"), trReport("Set")));

    info.insert(QStringLiteral("codecs"), codecs);
    info.insert(QStringLiteral("encoders"), encoders);
    info.insert(QStringLiteral("system"), system);
    info.insert(QStringLiteral("version"), QStringLiteral(DRIFT_VERSION));
    info.insert(QStringLiteral("package"), package);
    info.insert(QStringLiteral("vaapiAvailable"), vaapiOk);
    return info;
}

QString DebugReport::formatPlainText(const QVariantMap &info)
{
    QString text;
    text += QStringLiteral("CutWire Drift debug report\n\n");

    text += QStringLiteral("## System\n");
    const QVariantList system = info.value(QStringLiteral("system")).toList();
    for (const QVariant &entry : system) {
        const QVariantMap row = entry.toMap();
        text += QStringLiteral("- %1: %2\n")
                    .arg(row.value(QStringLiteral("label")).toString(),
                         row.value(QStringLiteral("value")).toString());
    }

    auto appendCodecTable = [&](const QString &heading, const QString &swHeader, const QString &hwHeader,
                                const QString &listKey, const QString &swNameKey, const QString &hwNameKey) {
        text += QStringLiteral("\n## %1\n").arg(heading);
        text += QStringLiteral("| Codec | %1 | %2 | SW | HW |\n").arg(swHeader, hwHeader);
        text += QStringLiteral("| --- | --- | --- | --- | --- |\n");
        const QVariantList rows = info.value(listKey).toList();
        for (const QVariant &entry : rows) {
            const QVariantMap row = entry.toMap();
            const QString hardware = row.value(QStringLiteral("hardwareUnavailable")).toBool()
                                         ? trReport("Unavailable")
                                         : supportLabel(row.value(QStringLiteral("hardware")).toBool());
            text += QStringLiteral("| %1 | %2 | %3 | %4 | %5 |\n")
                        .arg(row.value(QStringLiteral("name")).toString(),
                             supportLabel(row.value(QStringLiteral("software")).toBool()), hardware,
                             row.value(swNameKey).toString(), row.value(hwNameKey).toString());
        }
    };

    appendCodecTable(QStringLiteral("Video decoders"), QStringLiteral("Software"), QStringLiteral("Hardware"),
                     QStringLiteral("codecs"), QStringLiteral("softwareDecoder"),
                     QStringLiteral("hardwareDecoder"));
    appendCodecTable(QStringLiteral("Video encoders"), QStringLiteral("Software"), QStringLiteral("Hardware"),
                     QStringLiteral("encoders"), QStringLiteral("softwareEncoder"),
                     QStringLiteral("hardwareEncoder"));
    return text;
}
