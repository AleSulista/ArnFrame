#pragma once

#include <QString>

class MediaThumbnail
{
public:
    static constexpr int kFilmstripFrameWidth = 120;
    static constexpr int kFilmstripFrameHeight = 68;
    static constexpr int kFilmstripFrameCount = 8;

    static QString generate(const QString &sourcePath, const QString &kind);
    static QString generateFilmstrip(const QString &sourcePath, const QString &kind);
};
