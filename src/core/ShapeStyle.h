#pragma once

#include <QColor>
#include <QString>

namespace drift {

enum class ShapeKind { Rectangle, Square, Triangle, Pentagon, Hexagon };

QString shapeKindToString(ShapeKind kind);
ShapeKind shapeKindFromString(const QString &kind);

struct ShapeStyle
{
    ShapeKind kind = ShapeKind::Rectangle;
    QColor fill = QColor(0, 180, 255);
    QColor stroke = Qt::white;
    double strokeWidth = 4.0;
};

} // namespace drift
