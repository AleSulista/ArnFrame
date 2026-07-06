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

// The face warp effects do nothing without anchors, so their thumbnails need a face and a track
// to go with it. A drawn one keeps thumbnail generation reproducible and puts no real person's
// likeness in the repo — and because we place the features ourselves, the anchors are exact
// rather than detected.
QImage makeFaceBase(int size, drift::FaceAnchors *anchorsOut)
{
    const double S = size;
    QImage image(size, size, QImage::Format_RGBA8888);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bg(0, 0, S, S);
    bg.setColorAt(0.0, QColor(38, 44, 62));
    bg.setColorAt(1.0, QColor(20, 22, 32));
    p.fillRect(image.rect(), bg);

    // Faint stripes: a flat background hides the warp at the edge of the face.
    p.setPen(QPen(QColor(255, 255, 255, 16), S * 0.012));
    for (int i = -size; i < size * 2; i += int(S * 0.08))
        p.drawLine(QPointF(i, 0), QPointF(i + S, S));

    const QPointF center(0.5 * S, 0.5 * S);
    const double rx = 0.26 * S;
    const double ry = 0.32 * S;

    p.setPen(Qt::NoPen);
    QRadialGradient skin(center - QPointF(0, ry * 0.2), ry * 1.4);
    skin.setColorAt(0.0, QColor(226, 186, 152));
    skin.setColorAt(1.0, QColor(178, 132, 104));
    p.setBrush(skin);
    p.drawEllipse(center, rx, ry);

    const double eyeY = 0.42 * S;
    const double eyeDx = 0.11 * S;
    const double eyeR = 0.035 * S;
    for (int s = -1; s <= 1; s += 2) {
        const QPointF eye(center.x() + s * eyeDx, eyeY);
        p.setBrush(QColor(250, 250, 252));
        p.drawEllipse(eye, eyeR * 1.7, eyeR * 1.1);
        p.setBrush(QColor(60, 92, 120));
        p.drawEllipse(eye, eyeR, eyeR);
        p.setBrush(QColor(18, 18, 22));
        p.drawEllipse(eye, eyeR * 0.45, eyeR * 0.45);

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(78, 56, 44), S * 0.018, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(eye.x() - eyeR * 2.0, eye.y() - eyeR * 2.6, eyeR * 4.0, eyeR * 2.6),
                  20 * 16, 140 * 16);
        p.setPen(Qt::NoPen);
    }

    p.setPen(QPen(QColor(150, 108, 84), S * 0.016, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(center.x(), 0.47 * S), QPointF(center.x() - 0.02 * S, 0.55 * S));

    const double mouthY = 0.66 * S;
    const double mouthDx = 0.10 * S;
    p.setPen(QPen(QColor(150, 74, 74), S * 0.026, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(QRectF(center.x() - mouthDx, mouthY - 0.05 * S, mouthDx * 2.0, 0.10 * S),
              200 * 16, 140 * 16);
    p.end();

    drift::FaceAnchors a;
    a.valid = true;
    a.leftEye = QPointF(0.5 - 0.11, eyeY / S);
    a.rightEye = QPointF(0.5 + 0.11, eyeY / S);
    a.noseTip = QPointF(0.5, 0.55);
    a.mouthCenter = QPointF(0.5, mouthY / S);
    a.mouthLeft = QPointF(0.5 - 0.10, mouthY / S);
    a.mouthRight = QPointF(0.5 + 0.10, mouthY / S);
    a.chin = QPointF(0.5, 0.82);
    a.forehead = QPointF(0.5, 0.18);
    a.faceCenter = QPointF(0.5, 0.5);
    // The base is square, so width-normalized lengths are just the uv ones.
    a.faceRx = 0.26;
    a.faceRy = 0.32;
    a.angle = 0.0;
    a.eyeRadius = 0.035;
    a.score = 1.0;
    *anchorsOut = a;
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

    // "Face" selects which tracked person to follow, so pushing it toward its maximum like an
    // intensity slider just picks a face slot the thumbnail's single-face track does not have,
    // and every face effect renders as a pass-through.
    if (def.needsFace)
        params.insert(QStringLiteral("faceIndex"), 0.0);

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
    bool force = false;

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
        else if (a == QLatin1String("--force"))
            force = true;
        else if (a == QLatin1String("--help") || a == QLatin1String("-h")) {
            err << "usage: effectthumbs [--effects DIR] [--base image] [--only id] [--size N]\n"
                   "                    [--force]\n"
                   "\n"
                   "Writes thumbnail.png into each effect package directory. Packages that already\n"
                   "have one are left alone unless --force or --only names them: these files are\n"
                   "committed assets, and adding one effect must not rewrite all the others.\n";
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

    drift::FaceAnchors faceAnchors;
    const QImage faceBase = makeFaceBase(size, &faceAnchors);

    int ok = 0;
    int failed = 0;
    int skipped = 0;
    for (const EffectPresetEntry &def : effectCatalog()) {
        if (!onlyId.isEmpty() && def.meta.id != onlyId)
            continue;
        if (!def.isGpu || !def.gpu.valid) {
            err << "skip non-gpu " << def.meta.id << "\n";
            continue;
        }

        const QString outPath = QDir(def.gpu.packageDir).filePath(QStringLiteral("thumbnail.png"));

        // Thumbnails are committed assets. Regenerating every package because one new effect was
        // added rewrites 30-odd PNGs that nobody asked to change and buries the real diff, so an
        // existing file is only replaced when it was asked for by name or with --force.
        if (!force && onlyId.isEmpty() && QFile::exists(outPath)) {
            ++skipped;
            continue;
        }

        QImage result;
        const QMap<QString, QVariant> params = dramaticDefaults(def);
        if (def.meta.id == QLatin1String("time_echo")) {
            result = applyTimeEchoPreview(base, params);
        } else if (def.needsFace) {
            drift::Effect effect;
            effect.catalogId = def.meta.id;
            effect.parameters = params;

            result = EffectProcessor::applyEffects(faceBase, {effect}, 500000, {faceAnchors});
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

    out << "done: " << ok << " ok, " << failed << " failed, " << skipped << " kept\n";
    if (skipped > 0)
        out << "(pass --force to regenerate the ones that already have a thumbnail)\n";
    return failed == 0 ? 0 : 2;
}
