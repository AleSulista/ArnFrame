#include "TextStyle.h"

namespace drift {

QString textAlignToString(TextAlign align)
{
    switch (align) {
    case TextAlign::Left:
        return QStringLiteral("left");
    case TextAlign::Right:
        return QStringLiteral("right");
    case TextAlign::Center:
        return QStringLiteral("center");
    }
    return QStringLiteral("center");
}

TextAlign textAlignFromString(const QString &align)
{
    if (align == QStringLiteral("left"))
        return TextAlign::Left;
    if (align == QStringLiteral("right"))
        return TextAlign::Right;
    return TextAlign::Center;
}

} // namespace drift
