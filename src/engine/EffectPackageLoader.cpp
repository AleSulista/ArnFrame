#include "EffectPackageLoader.h"

#include "GpuEffectDefinition.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace {

void setError(QString *errorOut, EffectPresetEntry *entry, const QString &message)
{
    entry->gpu.errorMessage = message;
    if (errorOut)
        *errorOut = message;
}

QString readTextFile(const QString &path, QString *errorOut)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("cannot open %1").arg(path);
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool parsePassInput(const QJsonObject &obj, drift::GpuEffectPassInput *out, QString *errorOut)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("source_texture")) {
        out->type = drift::GpuEffectPassInput::Type::SourceTexture;
        return true;
    }
    if (type == QLatin1String("buffer")) {
        out->type = drift::GpuEffectPassInput::Type::Buffer;
        out->bufferId = obj.value(QStringLiteral("id")).toString();
        if (out->bufferId.isEmpty()) {
            if (errorOut)
                *errorOut = QStringLiteral("buffer input missing id");
            return false;
        }
        return true;
    }
    if (errorOut)
        *errorOut = QStringLiteral("unknown pass input type '%1'").arg(type);
    return false;
}

bool parsePassOutput(const QJsonObject &obj, drift::GpuEffectPassOutput *out, QString *errorOut)
{
    const QString type = obj.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("canvas")) {
        out->type = drift::GpuEffectPassOutput::Type::Canvas;
        return true;
    }
    if (type == QLatin1String("buffer")) {
        out->type = drift::GpuEffectPassOutput::Type::Buffer;
        out->bufferId = obj.value(QStringLiteral("id")).toString();
        if (out->bufferId.isEmpty()) {
            if (errorOut)
                *errorOut = QStringLiteral("buffer output missing id");
            return false;
        }
        return true;
    }
    if (errorOut)
        *errorOut = QStringLiteral("unknown pass output type '%1'").arg(type);
    return false;
}

QString slugifyCategory(const QString &raw)
{
    QString slug = raw.trimmed().toLower();
    slug.replace(QLatin1Char('&'), QLatin1Char(' '));
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    while (slug.startsWith(QLatin1Char('_')))
        slug.remove(0, 1);
    while (slug.endsWith(QLatin1Char('_')))
        slug.chop(1);
    return slug.isEmpty() ? QStringLiteral("misc") : slug;
}

bool parseParameters(const QJsonArray &params, EffectPresetEntry *entry, QString *errorOut, bool gpuBackend)
{
    for (const QJsonValue &pv : params) {
        const QJsonObject p = pv.toObject();
        drift::EffectParamSpec spec;
        spec.key = p.value(QStringLiteral("identifier")).toString();
        if (spec.key.isEmpty())
            spec.key = p.value(QStringLiteral("key")).toString();
        spec.label = p.value(QStringLiteral("displayName")).toString();
        if (spec.label.isEmpty())
            spec.label = p.value(QStringLiteral("label")).toString();
        if (spec.label.isEmpty())
            spec.label = spec.key;
        const QString type = p.value(QStringLiteral("type")).toString(QStringLiteral("float"));
        spec.isBoolean = (type == QLatin1String("bool") || type == QLatin1String("boolean"));
        spec.min = p.value(QStringLiteral("minValue")).toDouble(p.value(QStringLiteral("min")).toDouble(0.0));
        spec.max = p.value(QStringLiteral("maxValue")).toDouble(p.value(QStringLiteral("max")).toDouble(1.0));
        spec.defaultValue =
            p.value(QStringLiteral("defaultValue")).toDouble(p.value(QStringLiteral("default")).toDouble(0.0));

        if (spec.key.isEmpty()) {
            setError(errorOut, entry, QStringLiteral("parameter missing identifier"));
            return false;
        }
        if (gpuBackend && drift::isReservedGpuUniform(spec.key)) {
            setError(errorOut, entry,
                     QStringLiteral("parameter '%1' collides with reserved uniform").arg(spec.key));
            return false;
        }
        entry->meta.parameters.append(spec);
    }
    return true;
}

QVariant jsonToVariant(const QJsonValue &value)
{
    if (value.isBool())
        return value.toBool();
    if (value.isDouble())
        return value.toDouble();
    if (value.isString())
        return value.toString();
    return value.toVariant();
}

bool parseFixedParams(const QJsonObject &obj, EffectPresetEntry *entry)
{
    for (auto it = obj.begin(); it != obj.end(); ++it)
        entry->fixedParams.insert(it.key(), jsonToVariant(it.value()));
    return true;
}

bool loadGpuPipeline(const QJsonObject &root, const QString &packageDir, EffectPresetEntry *entry,
                     QString *errorOut)
{
    const QJsonObject pipeline = root.value(QStringLiteral("pipeline")).toObject();
    const QJsonArray buffers = pipeline.value(QStringLiteral("intermediateBuffers")).toArray();
    for (const QJsonValue &bv : buffers) {
        const QJsonObject b = bv.toObject();
        drift::GpuEffectBufferSpec buf;
        buf.id = b.value(QStringLiteral("id")).toString();
        buf.scale = b.value(QStringLiteral("scale")).toDouble(1.0);
        if (buf.id.isEmpty()) {
            setError(errorOut, entry, QStringLiteral("intermediate buffer missing id"));
            return false;
        }
        if (buf.scale <= 0.0) {
            setError(errorOut, entry, QStringLiteral("intermediate buffer scale must be > 0"));
            return false;
        }
        entry->gpu.intermediateBuffers.append(buf);
    }

    QSet<QString> bufferIds;
    for (const drift::GpuEffectBufferSpec &b : entry->gpu.intermediateBuffers)
        bufferIds.insert(b.id);

    const QJsonArray passes = pipeline.value(QStringLiteral("passes")).toArray();
    if (passes.isEmpty()) {
        setError(errorOut, entry, QStringLiteral("pipeline has no passes"));
        return false;
    }

    int index = 0;
    for (const QJsonValue &pv : passes) {
        const QJsonObject p = pv.toObject();
        drift::GpuEffectPass pass;
        pass.passIndex = p.value(QStringLiteral("passIndex")).toInt(index);
        pass.fragmentShaderFile = p.value(QStringLiteral("fragmentShader")).toString();
        if (pass.fragmentShaderFile.isEmpty()) {
            setError(errorOut, entry, QStringLiteral("pass %1 missing fragmentShader").arg(index));
            return false;
        }

        const QString shaderPath = QDir(packageDir).filePath(pass.fragmentShaderFile);
        if (!QFileInfo::exists(shaderPath)) {
            setError(errorOut, entry,
                     QStringLiteral("missing shader file '%1'").arg(pass.fragmentShaderFile));
            return false;
        }
        QString shaderError;
        pass.fragmentShaderSource = readTextFile(shaderPath, &shaderError);
        if (pass.fragmentShaderSource.isEmpty()) {
            setError(errorOut, entry,
                     shaderError.isEmpty() ? QStringLiteral("empty shader '%1'").arg(pass.fragmentShaderFile)
                                           : shaderError);
            return false;
        }

        const QJsonArray inputs = p.value(QStringLiteral("inputs")).toArray();
        if (inputs.isEmpty()) {
            drift::GpuEffectPassInput src;
            src.type = drift::GpuEffectPassInput::Type::SourceTexture;
            pass.inputs.append(src);
        } else {
            for (const QJsonValue &iv : inputs) {
                drift::GpuEffectPassInput input;
                QString inputError;
                if (!parsePassInput(iv.toObject(), &input, &inputError)) {
                    setError(errorOut, entry, inputError);
                    return false;
                }
                if (input.type == drift::GpuEffectPassInput::Type::Buffer
                    && !bufferIds.contains(input.bufferId)) {
                    setError(errorOut, entry,
                             QStringLiteral("pass references unknown buffer '%1'").arg(input.bufferId));
                    return false;
                }
                pass.inputs.append(input);
            }
        }

        const QJsonObject outputObj = p.value(QStringLiteral("output")).toObject();
        QString outputError;
        if (outputObj.isEmpty()) {
            if (index == passes.size() - 1) {
                pass.output.type = drift::GpuEffectPassOutput::Type::Canvas;
            } else {
                setError(errorOut, entry, QStringLiteral("pass %1 missing output").arg(index));
                return false;
            }
        } else if (!parsePassOutput(outputObj, &pass.output, &outputError)) {
            setError(errorOut, entry, outputError);
            return false;
        }

        if (pass.output.type == drift::GpuEffectPassOutput::Type::Buffer
            && !bufferIds.contains(pass.output.bufferId)) {
            setError(errorOut, entry,
                     QStringLiteral("pass output references unknown buffer '%1'").arg(pass.output.bufferId));
            return false;
        }

        entry->gpu.passes.append(pass);
        ++index;
    }

    entry->gpu.valid = true;
    return true;
}

} // namespace

EffectPresetEntry EffectPackageLoader::loadPackage(const QString &packageDir, QString *errorOut)
{
    EffectPresetEntry entry;
    entry.gpu.packageDir = packageDir;
    if (errorOut)
        errorOut->clear();

    const QString jsonPath = QDir(packageDir).filePath(QStringLiteral("effect.json"));
    QString readError;
    const QString jsonText = readTextFile(jsonPath, &readError);
    if (jsonText.isEmpty()) {
        setError(errorOut, &entry, readError.isEmpty() ? QStringLiteral("empty effect.json") : readError);
        return entry;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(errorOut, &entry, QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
        return entry;
    }

    const QJsonObject root = doc.object();
    const QString backend = root.value(QStringLiteral("backend")).toString();
    if (backend != QLatin1String("gpu") && backend != QLatin1String("libav")
        && backend != QLatin1String("compositor")) {
        setError(errorOut, &entry,
                 backend.isEmpty() ? QStringLiteral("effect.json missing backend")
                                   : QStringLiteral("unsupported backend '%1'").arg(backend));
        return entry;
    }

    entry.meta.id = root.value(QStringLiteral("id")).toString();
    entry.meta.displayName = root.value(QStringLiteral("displayName")).toString();
    if (entry.meta.displayName.isEmpty())
        entry.meta.displayName = entry.meta.id;
    const QString categoryRaw = root.value(QStringLiteral("category")).toString();
    entry.meta.category = slugifyCategory(categoryRaw.isEmpty() ? QStringLiteral("dreamy") : categoryRaw);
    entry.catalogOrder = root.value(QStringLiteral("order")).toInt(0);

    if (entry.meta.id.isEmpty()) {
        setError(errorOut, &entry, QStringLiteral("effect.json missing id"));
        return entry;
    }

    if (!parseParameters(root.value(QStringLiteral("parameters")).toArray(), &entry, errorOut,
                         backend == QLatin1String("gpu"))) {
        return entry;
    }

    if (root.contains(QStringLiteral("fixedParams")))
        parseFixedParams(root.value(QStringLiteral("fixedParams")).toObject(), &entry);

    if (backend == QLatin1String("gpu")) {
        entry.isGpu = true;
        entry.filterName = QStringLiteral("gpu");
        if (!loadGpuPipeline(root, packageDir, &entry, errorOut))
            return entry;
        return entry;
    }

    if (backend == QLatin1String("compositor")) {
        entry.meta.compositorOnly = true;
        entry.filterName.clear();
        return entry;
    }

    // libav
    entry.filterName = root.value(QStringLiteral("filterName")).toString();
    entry.graphTemplate = root.value(QStringLiteral("graphTemplate")).toString();
    if (entry.filterName.isEmpty() && entry.graphTemplate.isEmpty()) {
        setError(errorOut, &entry, QStringLiteral("libav effect needs filterName or graphTemplate"));
        return entry;
    }
    return entry;
}

QList<EffectPresetEntry> EffectPackageLoader::scanDirectories(const QStringList &rootDirs)
{
    QList<EffectPresetEntry> loaded;
    QSet<QString> seenIds;

    for (const QString &root : rootDirs) {
        if (root.isEmpty())
            continue;
        QDir dir(root);
        if (!dir.exists())
            continue;

        const QFileInfoList subdirs =
            dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &info : subdirs) {
            const QString packageDir = info.absoluteFilePath();
            if (!QFileInfo::exists(QDir(packageDir).filePath(QStringLiteral("effect.json"))))
                continue;

            QString error;
            EffectPresetEntry entry = loadPackage(packageDir, &error);
            if (!error.isEmpty() || entry.meta.id.isEmpty()) {
                qWarning("EffectPackageLoader: skip %s: %s", qPrintable(packageDir),
                         qPrintable(error.isEmpty() ? QStringLiteral("invalid package") : error));
                continue;
            }
            if (seenIds.contains(entry.meta.id)) {
                qWarning("EffectPackageLoader: duplicate id '%s' in %s (ignored)",
                         qPrintable(entry.meta.id), qPrintable(packageDir));
                continue;
            }
            seenIds.insert(entry.meta.id);
            loaded.append(entry);
        }
    }

    std::sort(loaded.begin(), loaded.end(), [](const EffectPresetEntry &a, const EffectPresetEntry &b) {
        if (a.catalogOrder != b.catalogOrder)
            return a.catalogOrder < b.catalogOrder;
        return a.meta.id < b.meta.id;
    });
    return loaded;
}

QStringList EffectPackageLoader::defaultSearchPaths()
{
    QStringList paths;

    if (const QByteArray env = qgetenv("DRIFT_EFFECTS_DIR"); !env.isEmpty()) {
        const QStringList parts =
            QString::fromUtf8(env).split(QDir::listSeparator(), Qt::SkipEmptyParts);
        for (const QString &p : parts)
            paths.append(QDir::cleanPath(p));
    }

    if (QCoreApplication::instance()) {
        const QString appDir = QCoreApplication::applicationDirPath();
        if (!appDir.isEmpty())
            paths.append(QDir(appDir).filePath(QStringLiteral("effects")));
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty())
        paths.append(QDir(appData).filePath(QStringLiteral("effects")));

    paths.removeDuplicates();
    return paths;
}
