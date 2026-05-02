#include "Track.h"

namespace drift {

QString trackTypeToString(TrackType type)
{
    switch (type) {
    case TrackType::Video:
        return QStringLiteral("video");
    case TrackType::Audio:
        return QStringLiteral("audio");
    case TrackType::Text:
        return QStringLiteral("text");
    case TrackType::Shape:
        return QStringLiteral("shape");
    }
    return QStringLiteral("video");
}

TrackType trackTypeFromString(const QString &type)
{
    if (type == QStringLiteral("audio"))
        return TrackType::Audio;
    if (type == QStringLiteral("text"))
        return TrackType::Text;
    if (type == QStringLiteral("shape"))
        return TrackType::Shape;
    return TrackType::Video;
}

bool Track::allowsClipType(ClipType clipType) const
{
    switch (type) {
    case TrackType::Audio:
        return clipType == ClipType::Audio;
    case TrackType::Text:
        return clipType == ClipType::Text;
    case TrackType::Shape:
        return clipType == ClipType::Image || clipType == ClipType::Shape;
    case TrackType::Video:
        return clipType == ClipType::Video;
    }
    return false;
}

} // namespace drift
