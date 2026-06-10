#pragma once

#include <QVariantList>
#include <QString>

class MediaWaveform
{
public:
    static QVariantList peaks(const QString &sourcePath, int sampleCount = 120);

    // Voice-emphasized peaks from already-decoded interleaved-stereo float PCM.
    // Collapses to mono, band-passes to the speech range (~150-3500 Hz), then
    // buckets max-abs amplitude into `buckets` normalized peaks in [0.05, 1.0].
    static QVariantList voicePeaksFromPcm(const float *interleavedStereo, int frameCount,
                                          int sampleRate, int buckets);
};
