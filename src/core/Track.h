#pragma once

#include "Clip.h"

#include <QList>
#include <QString>

namespace drift {

enum class TrackType { Video, Audio, Text };

QString trackTypeToString(TrackType type);
TrackType trackTypeFromString(const QString &type);

struct Track
{
    TrackType type = TrackType::Video;
    QList<Clip> clips;
    bool muted = false;
    bool hidden = false;
    bool locked = false;

    bool allowsClipType(ClipType clipType) const;
};

} // namespace drift
