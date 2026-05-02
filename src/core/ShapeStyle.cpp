#include "ShapeStyle.h"

namespace drift {

QString shapeKindToString(ShapeKind kind)
{
    switch (kind) {
    case ShapeKind::Rectangle:
        return QStringLiteral("rectangle");
    case ShapeKind::Square:
        return QStringLiteral("square");
    case ShapeKind::Triangle:
        return QStringLiteral("triangle");
    case ShapeKind::Pentagon:
        return QStringLiteral("pentagon");
    case ShapeKind::Hexagon:
        return QStringLiteral("hexagon");
    }
    return QStringLiteral("rectangle");
}

ShapeKind shapeKindFromString(const QString &kind)
{
    if (kind == QStringLiteral("square"))
        return ShapeKind::Square;
    if (kind == QStringLiteral("triangle"))
        return ShapeKind::Triangle;
    if (kind == QStringLiteral("pentagon"))
        return ShapeKind::Pentagon;
    if (kind == QStringLiteral("hexagon"))
        return ShapeKind::Hexagon;
    return ShapeKind::Rectangle;
}

} // namespace drift
