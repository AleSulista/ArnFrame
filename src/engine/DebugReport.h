#pragma once

#include <QString>
#include <QVariantMap>

// Snapshot of decoder/encoder capability and host facts for the Debug info
// dialog and GitHub bug reports. Hardware decode is VAAPI (Windows/macOS
// report none). Hardware encode is not wired up, so those cells stay
// unavailable even if FFmpeg has NVENC/QSV/AMF/VAAPI encoders.
class DebugReport
{
public:
    static QVariantMap collect();
    static QString formatPlainText(const QVariantMap &info);
};
