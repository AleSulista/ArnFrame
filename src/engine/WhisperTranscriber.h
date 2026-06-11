#pragma once

#include "core/SubtitleCue.h"

#include <QList>
#include <QString>

#include <functional>
#include <memory>
#include <vector>

namespace drift {

struct WhisperResult
{
    QList<SubtitleCue> cues; // times relative to the audio start (µs)
    bool cancelled = false;
    bool ok = false;
    QString error;
};

// Whisper (openai/whisper-small) speech-to-text on ONNX Runtime. Lazily loads the ~750 MB
// model set once and reuses it. All work is synchronous on the calling thread — callers run
// it off the GUI thread (see AppController::generateSubtitlesForClip).
class WhisperTranscriber
{
public:
    static WhisperTranscriber &instance();

    // Resolves the model directory and loads the sessions on first use. False if the models
    // are missing or failed to load (see lastError()).
    bool available();
    QString lastError() const;

    // pcm: 16 kHz mono float32. progress(fraction in [0,1]) returns false to request cancel.
    WhisperResult transcribe(const std::vector<float> &pcm,
                             const std::function<bool(double)> &progress);

    WhisperTranscriber(const WhisperTranscriber &) = delete;
    WhisperTranscriber &operator=(const WhisperTranscriber &) = delete;

private:
    WhisperTranscriber();
    ~WhisperTranscriber();

    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace drift
