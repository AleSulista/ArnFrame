#pragma once

#include <QString>
#include <QVariantList>
#include <QVector>

class MediaWaveform
{
public:
    // Max-abs amplitude peaks for the whole file at a fixed rate, plus the duration they
    // span so callers can map a source second back to an index. Values are raw [0, 1];
    // the display floor is applied by the caller.
    struct Dense
    {
        QVector<float> peaks;
        double durationSeconds = 0.0;
    };

    // Resolution is duration-proportional so a multi-hour file does not collapse into the
    // same handful of buckets as a short one. `maxPeaks` bounds memory (1M floats = 4 MB).
    static Dense densePeaks(const QString &sourcePath, int peaksPerSecond = 100,
                            int maxPeaks = 1 << 20);

    // Voice-emphasized peaks from already-decoded interleaved-stereo float PCM.
    // Collapses to mono, band-passes to the speech range (~150-3500 Hz), then
    // buckets max-abs amplitude into `buckets` normalized peaks in [0.05, 1.0].
    static QVariantList voicePeaksFromPcm(const float *interleavedStereo, int frameCount,
                                          int sampleRate, int buckets);
};
