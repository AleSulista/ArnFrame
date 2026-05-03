#pragma once

#include <QMap>
#include <QString>
#include <QVariant>

namespace drift {

// Per-clip libavfilter effect with named parameters.
struct Effect
{
    QString name;
    QMap<QString, QVariant> parameters;

    // Id into the effect preset catalog (src/engine/EffectCatalog.h) this effect was
    // created from; UI-only bookkeeping so the inspector can show the right
    // label/sliders. Rendering uses catalog metadata in the engine layer.
    QString catalogId;

    QString filterGraphString() const;
};

} // namespace drift
