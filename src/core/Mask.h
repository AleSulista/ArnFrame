#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>

namespace drift {

enum class MaskShape { None, Rectangle, Ellipse, Star, Heart, Bars, Freeform };

QString maskShapeToString(MaskShape shape);
MaskShape maskShapeFromString(const QString &shape);

struct Mask
{
    MaskShape shape = MaskShape::None;
    double x = 0.5; // center, normalized
    double y = 0.5;
    double w = 0.6; // size, normalized
    double h = 0.6;
    double rotation = 0.0;
    double feather = 0.0; // px blur on the alpha edge
    bool invert = false;
    QVector<QPointF> points; // normalized, for Freeform
};

} // namespace drift
