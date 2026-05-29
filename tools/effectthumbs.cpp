#include "engine/EffectCatalog.h"
#include "engine/EffectPackageLoader.h"
#include "engine/EffectProcessor.h"
#include "engine/GpuEffectExecutor.h"

#include <QGuiApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QTextStream>

namespace {

QImage makeFallbackBase(int size)
{
    QImage image(size, size, QImage::Format_RGBA8888);
    image.fill(QColor(24, 26, 32));
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient bg(0, 0, size, size);
    bg.setColorAt(0.0, QColor(28, 32, 44));
    bg.setColorAt(1.0, QColor(18, 18, 24));
    p.fillRect(image.rect(), bg);

    const QPoint center(size / 2, int(size * 0.42));
    const int headR = int(size * 0.18);
    QRadialGradient skin(center, headR);
    skin.setColorAt(0.0, QColor(214, 172, 140));
    skin.setColorAt(1.0, QColor(160, 118, 96));
    p.setBrush(skin);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, headR, headR);

    p.setBrush(QColor(52, 58, 74));
    p.drawRoundedRect(int(size * 0.28), int(size * 0.58), int(size * 0.44), int(size * 0.55),
                      int(size * 0.08), int(size * 0.08));
    p.end();
    return image;
}

QMap<QString, QVariant> dramaticDefaults(const EffectPresetEntry &def)
{
    QMap<QString, QVariant> params;
    for (const drift::EffectParamSpec &spec : def.meta.parameters) {
        double v = spec.defaultValue;
        // Push slider effects toward a readable preview when defaults are subtle.
        if (!spec.isBoolean && spec.max > spec.min) {
            const double mid = (spec.min + spec.max) * 0.5;
            if (qFuzzyCompare(v, 0.0) || qFuzzyCompare(v, 1.0))
                v = mid + (spec.max - mid) * 0.45;
            else
                v = spec.min + (spec.max - spec.min) * 0.7;
            v = qBound(spec.min, v, spec.max);
        }
        params.insert(spec.key, v);
    }

    // Known strong showcase overrides.
    if (def.meta.id == QLatin1String("adjust.contrast"))
        params.insert(QStringLiteral("contrast"), 1.8);
    else if (def.meta.id == QLatin1String("adjust.brightness"))
        params.insert(QStringLiteral("brightness"), 0.35);
    else if (def.meta.id == QLatin1String("adjust.saturation"))
        params.insert(QStringLiteral("saturation"), 2.0);
    else if (def.meta.id == QLatin1String("rgb_split"))
        params.insert(QStringLiteral("amount"), 12.0);
    else if (def.meta.id == QLatin1String("stylize.pixelate")) {
        params.insert(QStringLiteral("width"), 24.0);
        params.insert(QStringLiteral("height"), 24.0);
    } else if (def.meta.id == QLatin1String("time_echo")) {
        params.insert(QStringLiteral("frames"), 4);
        params.insert(QStringLiteral("decay"), 0.65);
    }
    return params;
}

QImage applyTimeEchoPreview(const QImage &base, const QMap<QString, QVariant> &params)
{
    QList<QImage> frames;
    frames.reserve(5);
    frames.append(base);
    for (int i = 1; i <= 4; ++i) {
        QImage shifted(base.size(), QImage::Format_RGBA8888);
        shifted.fill(Qt::transparent);
        QPainter p(&shifted);
        p.drawImage(QPoint(i * 4, 0), base);
        p.end();
        frames.append(shifted);
    }
    const double decay = params.value(QStringLiteral("decay"), 0.65).toDouble();
    const int blendMode = 0;
    QImage gpu = GpuEffectExecutor::instance().blendTimeEcho(frames, decay, blendMode);
    return gpu.isNull() ? base : gpu;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QTextStream err(stderr);
    QTextStream out(stdout);

    const QStringList args = app.arguments();
    QString effectsRoot = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("effects"));
    QString basePath;
    QString onlyId;
    int size = 256;

    for (int i = 1; i < args.size(); ++i) {
        const QString a = args.at(i);
        if (a == QLatin1String("--effects") && i + 1 < args.size())
            effectsRoot = args.at(++i);
        else if (a == QLatin1String("--base") && i + 1 < args.size())
            basePath = args.at(++i);
        else if (a == QLatin1String("--only") && i + 1 < args.size())
            onlyId = args.at(++i);
        else if (a == QLatin1String("--size") && i + 1 < args.size())
            size = qBound(64, args.at(++i).toInt(), 1024);
        else if (a == QLatin1String("--help") || a == QLatin1String("-h")) {
            err << "usage: effectthumbs [--effects DIR] [--base image] [--only id] [--size N]\n";
            return 0;
        }
    }

    if (!QDir(effectsRoot).exists()) {
        err << "effects dir missing: " << effectsRoot << "\n";
        return 1;
    }

    reloadEffectCatalog({effectsRoot});
    if (!GpuEffectExecutor::instance().isAvailable()) {
        err << "OpenGL offscreen context unavailable\n";
        return 1;
    }

    QImage base;
    if (!basePath.isEmpty())
        base = QImage(basePath).convertToFormat(QImage::Format_RGBA8888);
    if (base.isNull())
        base = makeFallbackBase(size * 2);
    base = base.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
               .copy(0, 0, size, size);

    int ok = 0;
    int failed = 0;
    for (const EffectPresetEntry &def : effectCatalog()) {
        if (!onlyId.isEmpty() && def.meta.id != onlyId)
            continue;
        if (!def.isGpu || !def.gpu.valid) {
            err << "skip non-gpu " << def.meta.id << "\n";
            continue;
        }

        const QString outPath = QDir(def.gpu.packageDir).filePath(QStringLiteral("thumbnail.png"));
        QImage result;
        const QMap<QString, QVariant> params = dramaticDefaults(def);
        if (def.meta.id == QLatin1String("time_echo")) {
            result = applyTimeEchoPreview(base, params);
        } else {
            drift::Effect effect;
            effect.catalogId = def.meta.id;
            effect.parameters = params;
            result = EffectProcessor::applyEffects(base, {effect}, 500000);
        }

        if (result.isNull()) {
            err << "FAIL " << def.meta.id << "\n";
            ++failed;
            continue;
        }

        result = result.convertToFormat(QImage::Format_RGBA8888)
                     .scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (!result.save(outPath, "PNG")) {
            err << "FAIL write " << outPath << "\n";
            ++failed;
            continue;
        }
        out << "wrote " << outPath << "\n";
        ++ok;
    }

    out << "done: " << ok << " ok, " << failed << " failed\n";
    return failed == 0 ? 0 : 2;
}
