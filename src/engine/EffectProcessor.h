#pragma once

#include "core/Effect.h"
#include "core/Time.h"

#include <QImage>
#include <QList>

// Applies per-clip libavfilter effects to a single RGBA frame.
class EffectProcessor
{
public:
    static QImage applyEffects(const QImage &input, const QList<drift::Effect> &effects,
                               drift::TimeUs timeUs = 0);
};
