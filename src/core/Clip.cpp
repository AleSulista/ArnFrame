#include "Clip.h"

namespace drift {

QString clipTypeToString(ClipType type)
{
    switch (type) {
    case ClipType::Video:
        return QStringLiteral("video");
    case ClipType::Audio:
        return QStringLiteral("audio");
    case ClipType::Image:
        return QStringLiteral("image");
    case ClipType::Text:
        return QStringLiteral("text");
    }
    return QStringLiteral("video");
}

ClipType clipTypeFromString(const QString &type)
{
    if (type == QStringLiteral("audio"))
        return ClipType::Audio;
    if (type == QStringLiteral("image"))
        return ClipType::Image;
    if (type == QStringLiteral("text"))
        return ClipType::Text;
    return ClipType::Video;
}

} // namespace drift
