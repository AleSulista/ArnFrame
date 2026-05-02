#pragma once

#include "Effect.h"
#include "Keyframe.h"
#include "MediaAsset.h"
#include "ShapeStyle.h"
#include "TextStyle.h"
#include "Time.h"

#include <QList>
#include <QString>

namespace drift {

enum class ClipType { Video, Audio, Image, Text, Shape };

QString clipTypeToString(ClipType type);
ClipType clipTypeFromString(const QString &type);

enum class BlendMode { Normal, Multiply, Screen, Overlay, Add, Darken, Lighten };

QString blendModeToString(BlendMode mode);
BlendMode blendModeFromString(const QString &mode);

struct Clip
{
    QString id;
    QString assetId;
    ClipType type = ClipType::Video;

    TimeUs timelineStart = 0;
    TimeUs timelineDuration = 0;
    TimeUs srcIn = 0;
    TimeUs srcOut = 0;

    QString name;
    QString textContent;
    TextStyle textStyle; // only meaningful when type == Text
    ShapeStyle shapeStyle; // only meaningful when type == Shape

    QString path;
    QString thumbnailPath;
    QString filmstripPath;

    BlendMode blendMode = BlendMode::Normal;

    KeyframeTrack<double> opacity;
    KeyframeTrack<double> posX;
    KeyframeTrack<double> posY;
    KeyframeTrack<double> scale;
    KeyframeTrack<double> rotation;
    KeyframeTrack<double> volume;
    QList<Effect> effects;

    TimeUs timelineEnd() const { return timelineStart + timelineDuration; }

    bool containsTime(TimeUs time) const
    {
        return time >= timelineStart && time < timelineEnd();
    }
};

} // namespace drift
