#include "AudioMixer.h"

#include "ClipReaderPool.h"
#include "core/Clip.h"

#include <QtMath>
#include <cstring>

namespace {

float softClip(float sample)
{
    if (sample > 1.0f)
        return 1.0f;
    if (sample < -1.0f)
        return -1.0f;
    return sample;
}

double volumeForClip(const drift::Clip &clip, drift::TimeUs timelineUs)
{
    if (clip.volume.isEmpty())
        return 1.0;
    const drift::TimeUs relative = qMax<drift::TimeUs>(0, timelineUs - clip.timelineStart);
    return qBound(0.0, clip.volume.evaluateAt(relative), 2.0);
}

void accumulateClipAudio(const drift::Clip &clip, drift::TimeUs timelineStartUs, int sampleCount,
                         int sampleRate, float *mixBuffer)
{
    if (clip.path.isEmpty())
        return;

    const drift::TimeUs bufferEndUs = timelineStartUs + static_cast<drift::TimeUs>(
                                                            (static_cast<int64_t>(sampleCount) * drift::kUsPerSecond)
                                                            / sampleRate);

    if (!clip.containsTime(timelineStartUs) && !clip.containsTime(bufferEndUs - 1))
        return;

    const drift::TimeUs clipOffsetUs = qMax<drift::TimeUs>(0, timelineStartUs - clip.timelineStart);
    const drift::TimeUs sourceStartUs = clip.srcIn + clipOffsetUs;

    QVector<float> chunk(sampleCount * 2);
    const int got = ClipReaderPool::instance().readAudioInterleaved(clip.path, sourceStartUs, sampleCount,
                                                                    sampleRate, chunk.data());
    if (got <= 0)
        return;

    const float gain = static_cast<float>(volumeForClip(clip, timelineStartUs));
    for (int i = 0; i < got * 2; ++i)
        mixBuffer[i] += chunk[i] * gain;
}

} // namespace

void AudioMixer::mix(drift::TimeUs timelineStartUs, int sampleCount, int sampleRate,
                     float *interleavedStereoOut) const
{
    if (!interleavedStereoOut || sampleCount <= 0 || !m_project)
        return;

    std::memset(interleavedStereoOut, 0, static_cast<size_t>(sampleCount) * 2 * sizeof(float));

    for (const drift::Track &track : m_project->tracks()) {
        if (track.muted || track.hidden)
            continue;

        if (track.type == drift::TrackType::Audio) {
            for (const drift::Clip &clip : track.clips)
                accumulateClipAudio(clip, timelineStartUs, sampleCount, sampleRate, interleavedStereoOut);
        } else if (track.type == drift::TrackType::Video) {
            for (const drift::Clip &clip : track.clips) {
                if (clip.type == drift::ClipType::Video)
                    accumulateClipAudio(clip, timelineStartUs, sampleCount, sampleRate, interleavedStereoOut);
            }
        }
    }

    for (int i = 0; i < sampleCount * 2; ++i)
        interleavedStereoOut[i] = softClip(interleavedStereoOut[i]);
}
