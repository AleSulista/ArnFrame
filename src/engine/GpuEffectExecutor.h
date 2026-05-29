#pragma once

#include "EffectCatalog.h"
#include "core/Time.h"

#include <QImage>
#include <QMap>
#include <QVariant>

// Offscreen OpenGL executor for file-based GPU effect packages.
// Grace mode: on init/compile/draw failure, returns the input image unchanged.
class GpuEffectExecutor
{
public:
    static GpuEffectExecutor &instance();

    // Apply one GPU catalog effect. Returns input unchanged on any failure.
    QImage apply(const EffectPresetEntry &def, const QImage &input,
                 const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs);

    // GPU blend for time_echo history (newest-first). Falls back to empty on GL failure
    // so callers can use a CPU path.
    QImage blendTimeEcho(const QList<QImage> &framesNewestFirst, double decay, int blendMode);

    bool isAvailable();

private:
    GpuEffectExecutor() = default;
    bool ensureContext();
    void releaseGl();

    bool m_triedInit = false;
    bool m_available = false;
};
