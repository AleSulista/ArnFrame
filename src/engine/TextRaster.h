#pragma once

#include "core/Clip.h"
#include "core/Time.h"

#include <QImage>
#include <QRectF>

// Text rasterization for the compositor. Glyphs are drawn on the CPU with QPainter and handed to
// the GPU as a layer texture, exactly like a decoded video frame.

struct TextRasterResult
{
    QImage image; // includes the bleed margin, so stroke/shadow/box are never cropped
    QRectF rect;  // destination rect in canvas px: the layout rect grown by that same bleed
};

// Rasterize (or return a cached copy of) the styled text. layoutRect is the clip's layout rect in
// canvas pixels; renderScale maps project pixels to canvas pixels.
TextRasterResult rasterizeText(const drift::Clip &clip, const QRectF &layoutRect, double renderScale);

// Entrance/exit motion, sampled at a timeline instant. Applied to the *layer* — never to the
// raster — so the cached texture stays valid for every frame of the animation.
struct TextAnimSample
{
    double opacity = 1.0;
    double dx = 0.0;
    double dy = 0.0;
    double scale = 1.0;
    double blurPx = 0.0;
};

TextAnimSample sampleTextAnimation(const drift::Clip &clip, drift::TimeUs timelineUs,
                                   const QRectF &layoutRect, double renderScale);

// Widest blur an entrance/exit can ask for, in project px. Reserved in the bleed margin up front so
// the image size — and therefore the cache key — does not change as the animation plays.
constexpr double kTextBlurMaxPx = 24.0;
