#pragma once

#include "core/Effect.h"
#include "core/EffectPreset.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

// Engine-side preset entry: core metadata plus libavfilter / compositor wiring.
struct EffectPresetEntry
{
    drift::EffectPresetMeta meta;
    QString filterName;                    // single-filter name, e.g. "eq"
    QString graphTemplate;                 // optional multi-filter template with {{key}} placeholders
    QMap<QString, QVariant> fixedParams;   // always applied, not exposed as sliders
};

// Backward-compatible alias used across the codebase.
using EffectParamDef = drift::EffectParamSpec;
using EffectDef = EffectPresetEntry;

const QList<EffectPresetEntry> &effectCatalog();
const EffectPresetEntry *effectDefForId(const QString &id);

// Merge fixed + instance parameters for a clip effect.
QMap<QString, QVariant> resolvedEffectParameters(const drift::Effect &effect, const EffectPresetEntry &def);

// Build a libavfilter graph fragment for one catalog effect instance (empty when compositor-only).
QString buildFilterGraphForEffect(const drift::Effect &effect, const EffectPresetEntry *def = nullptr);

// All stable preset ids (for tests and validation).
QStringList effectPresetIds();

// Browser categories in display order (stable id + user-facing label).
QList<QPair<QString, QString>> effectCategories();
QString effectCategoryLabel(const QString &categoryId);
