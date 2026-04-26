#pragma once

#include <QString>

// Extracts a preview frame (or scales a still image) and caches it on disk.
class MediaThumbnail
{
public:
    static QString generate(const QString &sourcePath, const QString &kind);
};
