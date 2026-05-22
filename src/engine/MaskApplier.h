#pragma once

#include "core/Mask.h"

#include <QImage>

namespace drift {

QImage applyMask(const QImage &frame, const Mask &mask, int canvasWidth, int canvasHeight);

} // namespace drift
