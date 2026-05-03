#pragma once

#include "core/Time.h"

#include <QImage>
#include <QMap>
#include <QString>
#include <QVariant>

// CPU compositor implementations for presets flagged compositorOnly in EffectCatalog.
namespace CompositorEffects {

QImage apply(const QString &presetId, const QImage &input, const QMap<QString, QVariant> &parameters,
             drift::TimeUs timeUs = 0);

} // namespace CompositorEffects
