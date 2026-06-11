// Headless smoke test for the Whisper auto-subtitle transcriber: decode a media file to
// 16 kHz mono and print the timed cues. Usage: transcribe <media-file>

#include "engine/ClipReaderPool.h"
#include "engine/MediaProbe.h"
#include "engine/WhisperTranscriber.h"
#include "core/Time.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QVector>

#include <vector>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() != 2) {
        err << "usage: transcribe <media-file>\n";
        return 1;
    }
    const QString path = args.at(1);

    const MediaInfo info = MediaProbe::probe(path);
    if (!info.ok || info.durationUs <= 0) {
        err << "probe failed: " << info.errorString << "\n";
        return 1;
    }

    const int sampleRate = 16000;
    const int frames = static_cast<int>((info.durationUs * sampleRate) / drift::kUsPerSecond);
    QVector<float> stereo(static_cast<qsizetype>(frames) * 2);
    const int got = ClipReaderPool::instance().readAudioInterleaved(path, 0, frames, sampleRate,
                                                                    stereo.data());
    if (got <= 0) {
        err << "no audio decoded\n";
        return 1;
    }

    std::vector<float> mono(got);
    for (int i = 0; i < got; ++i)
        mono[i] = 0.5f * (stereo[i * 2] + stereo[i * 2 + 1]);

    drift::WhisperTranscriber &w = drift::WhisperTranscriber::instance();
    if (!w.available()) {
        err << "whisper unavailable: " << w.lastError() << "\n";
        return 1;
    }

    const drift::WhisperResult res = w.transcribe(mono, [&](double p) {
        err << "\r" << static_cast<int>(p * 100) << "%   ";
        err.flush();
        return true;
    });
    err << "\n";

    if (!res.ok) {
        err << "transcription failed: " << res.error << "\n";
        return 1;
    }

    for (const drift::SubtitleCue &cue : res.cues) {
        out << QString::number(drift::usToSeconds(cue.startUs), 'f', 2) << " -> "
            << QString::number(drift::usToSeconds(cue.endUs), 'f', 2) << "  " << cue.text << "\n";
    }
    out << "(" << res.cues.size() << " cues)\n";
    return 0;
}
