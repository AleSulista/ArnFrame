#pragma once

#include "core/Clip.h"
#include "core/Effect.h"
#include "core/Mask.h"
#include "core/Time.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QMap>
#include <QRectF>
#include <QSize>
#include <QVariant>

namespace drift {
struct GpuEffectDefinition;
}

// A single textured layer: the clip's source pixels plus everything needed to
// place it on the canvas. `source` is CPU pixels (a decoded video frame, or a
// QPainter raster of a text/shape clip); the GPU does the scaling, rotation,
// masking and blending.
struct GpuLayer
{
    QImage source; // null => fully transparent layer
    QList<drift::Effect> effects;
    drift::Mask mask;
    QRectF rect;             // destination rect on the canvas, in canvas pixels
    double rotation = 0.0;   // degrees, clockwise, about the rect centre
    bool flipH = false;
    bool flipV = false;
    double opacity = 1.0;
    drift::TimeUs clipTimeUs = 0; // effect time base (relative to clip start)
    bool valid = false;
};

// One drawable in the scene: either a plain layer, or a transition that mixes
// two isolated layers through a shader.
struct GpuItem
{
    bool isTransition = false;
    drift::BlendMode blend = drift::BlendMode::Normal;

    GpuLayer layer; // when !isTransition

    GpuLayer from; // when isTransition
    GpuLayer to;
    QString transitionKey;
    const drift::GpuEffectDefinition *transitionGpu = nullptr;
    QMap<QString, QVariant> transitionParams;
    double progress = 0.0;
    drift::TimeUs transitionTimeUs = 0;
};

// Everything needed to render one composited frame, with no reference to the
// project model. FrameCompositor builds this (pure data, no pixels touched);
// GpuCompositor renders it.
struct GpuScene
{
    QSize canvasSize;
    QColor backgroundColor = Qt::black;
    bool backgroundBlur = false;
    double blurStrengthPx = 20.0;
    QImage blurSource; // topmost visual frame, already decoded
    QList<GpuItem> items; // back-to-front
};

// A composited frame still living in GPU memory. The texture belongs to the GL
// runtime's presentation ring — the receiver must not delete it, and must stop
// using it once a few more frames have been composited.
struct GpuFrameTexture
{
    unsigned int textureId = 0;
    QSize size;

    bool isValid() const { return textureId != 0 && !size.isEmpty(); }
};

// Composites a GpuScene entirely on the GPU. Returns a null image if OpenGL is
// unavailable, which lets FrameCompositor fall back to its CPU path.
namespace GpuCompositor {

// Composite and read back to a QImage. Used by export, thumbnails and tools.
QImage render(const GpuScene &scene);

// Composite and leave the result on the GPU. Used by the preview, which hands
// the texture straight to the scene graph — no readback, no re-upload.
GpuFrameTexture renderToTexture(const GpuScene &scene);

bool isAvailable();

} // namespace GpuCompositor
