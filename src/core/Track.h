#pragma once

#include "Clip.h"
#include "Transition.h"

#include <QList>
#include <QString>

namespace drift {

enum class TrackType { Video, Audio, Text, Subtitle, Shape };

QString trackTypeToString(TrackType type);
TrackType trackTypeFromString(const QString &type);

struct Track
{
    TrackType type = TrackType::Video;
    QList<Clip> clips;
    QList<Transition> transitions;
    bool muted = false;
    bool hidden = false;
    bool locked = false;
    bool showWaveform = false; // view-only: show audio waveform instead of filmstrip for this track's clips

    bool allowsClipType(ClipType clipType) const;
};

} // namespace drift
