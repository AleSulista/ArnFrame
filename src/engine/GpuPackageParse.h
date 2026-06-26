#pragma once

#include "GpuEffectDefinition.h"
#include "core/EffectPreset.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

// Shared JSON grammar for GPU packages. Effects (effect.json) and transitions
// (transition.json) declare parameters and pipelines identically; only the
// surrounding metadata differs.
namespace GpuPackageParse {

QString readTextFile(const QString &path, QString *errorOut);

QString slugifyCategory(const QString &raw);

QVariant jsonToVariant(const QJsonValue &value);

// Parse "parameters": [...]. Rejects identifiers colliding with reserved uniforms when gpuBackend.
bool parseParameters(const QJsonArray &params, QList<drift::EffectParamSpec> *out, bool gpuBackend,
                     QString *errorOut);

void parseFixedParams(const QJsonObject &obj, QMap<QString, QVariant> *out);

// Parse "pipeline": { intermediateBuffers, textures, passes }. Sets out->valid on success.
// maxSourceIndex bounds "source_texture" indices: 0 for effects, 1 for transitions.
bool loadGpuPipeline(const QJsonObject &root, const QString &packageDir, int maxSourceIndex,
                     drift::GpuEffectDefinition *out, QString *errorOut);

// Resolve an optional relative/absolute asset path inside a package; empty when missing.
QString resolvePackageAsset(const QString &packageDir, const QString &relOrAbs);

// Search roots for a package kind: $<envVar>, <appDir>/<subdir>, <AppDataLocation>/<subdir>, then
// the content roots of any installed addon providing `addonKind` (empty to skip). Earlier roots
// win, so a developer's DRIFT_*_DIR still shadows a downloaded addon.
QStringList defaultSearchPaths(const QString &envVar, const QString &subdir,
                               const QString &addonKind = {});

} // namespace GpuPackageParse
