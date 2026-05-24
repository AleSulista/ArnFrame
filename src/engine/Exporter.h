#pragma once

#include <QList>
#include <QString>

#include <functional>

namespace drift {
class Project;
}

// A named encode target. Resolution is derived from the project's aspect ratio so
// export never distorts; the preset only scales (by height), picks fps, and sets
// video bitrate. Audio is always AAC stereo at the project sample rate.
struct ExportPreset
{
    QString id;
    QString label;
    int targetHeight = 0;         // 0 = keep project height
    int fps = 0;                  // 0 = keep project fps
    int videoBitrateKbps = 12000;
};

// WYSIWYG exporter: encodes frames straight from FrameCompositor and audio from
// AudioMixer, so the exported file matches the preview exactly (single compositor).
class Exporter
{
public:
    // Called with progress in [0,1]; return false to cancel the export.
    using ProgressFn = std::function<bool(double)>;

    static const QList<ExportPreset> &presets();
    static const ExportPreset *presetById(const QString &id);

    static bool run(const drift::Project &project, const ExportPreset &preset, const QString &outputPath,
                    QString *errorOut, const ProgressFn &onProgress = {});
};
