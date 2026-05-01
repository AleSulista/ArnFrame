#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

// Declarative registry of one-click libavfilter effects, shared by the
// effects browser (grid of cards) and the per-clip effect stack (sliders).
struct EffectParamDef
{
    QString key;   // libavfilter option name, e.g. "contrast"
    QString label; // "Contrast"
    double min = 0.0;
    double max = 1.0;
    double def = 0.0;
};

struct EffectDef
{
    QString id;                       // "adjust.contrast"
    QString label;                    // "Contrast"
    QString category;                 // "adjustment" or "stylize"
    QString filterName;               // libavfilter name, e.g. "eq"
    QList<EffectParamDef> params;     // user-adjustable sliders
    QMap<QString, QVariant> fixedParams; // always applied, not exposed as sliders
};

const QList<EffectDef> &effectCatalog();
const EffectDef *effectDefForId(const QString &id);
