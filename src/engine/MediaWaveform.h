#pragma once

#include <QVariantList>
#include <QString>

class MediaWaveform
{
public:
    static QVariantList peaks(const QString &sourcePath, int sampleCount = 120);
};
