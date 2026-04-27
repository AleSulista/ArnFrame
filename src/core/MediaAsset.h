#pragma once

#include "Time.h"

#include <QString>

namespace drift {

enum class MediaKind { Video, Audio, Image, Other };

QString mediaKindToString(MediaKind kind);
MediaKind mediaKindFromString(const QString &kind);

// Referenced media file with probed metadata.
struct MediaAsset
{
    QString id;
    QString path;
    QString name;
    MediaKind kind = MediaKind::Other;
    TimeUs durationUs = 0;

    int width = 0;
    int height = 0;
    double fps = 0.0;
    int rotationDegrees = 0;

    int sampleRate = 0;
    int channels = 0;
    QString codecName;

    QString durationLabel;
    QString thumbnailPath;
    QString filmstripPath;
};

} // namespace drift
