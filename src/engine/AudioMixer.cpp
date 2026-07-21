#include "AudioMixer.h"

#include "AudioAtempo.h"
#include "AudioEffectChain.h"
#include "ClipReaderPool.h"
#include "TransitionCatalog.h"
#include "core/Clip.h"
#include "core/Transition.h"

#include <QtMath>
#include <cmath>
#include <cstring>
#include <memory>
#include <utility>

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

double transitionGainForClip(const drift::Track &track, const drift::Clip &clip, drift::TimeUs timelineUs)
{
    drift::TimeUs windowStart = 0;
    drift::TimeUs windowEnd = 0;
    const drift::Transition *transition = drift::activeTransitionAt(track, timelineUs, windowStart, windowEnd);
    if (!transition)
        return 1.0;

    const double p = drift::transitionProgress(timelineUs, windowStart, windowEnd);
    const TransitionPresetEntry *def = transitionDefForId(transition->kindId);
    const QString curve = def ? def->audioCurve : QStringLiteral("crossfade");
    const drift::TransitionAudioGains gains = drift::transitionAudioGains(curve, p);
    if (clip.id == transition->fromClipId)
        return gains.outgoing;
    if (clip.id == transition->toClipId)
        return gains.incoming;
    return 1.0;
}

constexpr drift::TimeUs kTimelineGapToleranceUs = 2'000; // ~2 ms: allow frame rounding between blocks

} // namespace

// Silence outside the clip means this is safe to call for a preroll window that runs off the
// clip's front edge.
QVector<float> AudioMixer::readClipAudio(const drift::Clip &clip, drift::TimeUs winStartUs, int outFrames,
                                         int sampleRate)
{
    QVector<float> out(outFrames * 2, 0.0f);
    if (outFrames <= 0)
        return out;

    const drift::TimeUs winDurUs =
        static_cast<drift::TimeUs>((static_cast<int64_t>(outFrames) * drift::kUsPerSecond) / sampleRate);
    const drift::TimeUs winEndUs = winStartUs + winDurUs;
    if (winEndUs <= clip.timelineStart || winStartUs >= clip.timelineEnd())
        return out; // window is entirely outside the clip — pure silence

    // Clamp the window to the clip; frames before the clip's start stay as the leading zeros above.
    const drift::TimeUs playStartUs = qMax(winStartUs, clip.timelineStart);
    const int leadFrames = static_cast<int>(((playStartUs - winStartUs) * sampleRate) / drift::kUsPerSecond);
    const int wantFrames = qMax(1, outFrames - leadFrames);

    const drift::TimeUs wantDurUs =
        static_cast<drift::TimeUs>((static_cast<int64_t>(wantFrames) * drift::kUsPerSecond) / sampleRate);

    // A ramp only means the rate changes from block to block; within one block it is read as
    // constant, which is what atempo can express anyway. Taken from the curve rather than by
    // differencing two mapped positions so the final, partly-overhanging block of a clip still
    // reports its true rate.
    const double speed =
        clip.hasSpeedCurve()
            ? clip.speedCurve.speedAtTimelineOffset(playStartUs - clip.timelineStart,
                                                    clip.srcOut - clip.srcIn)
            : clip.effectiveSpeed();
    const drift::TimeUs sourceSpanUs =
        qMax<drift::TimeUs>(1, static_cast<drift::TimeUs>(std::llround(wantDurUs * speed)));
    const int sourceSampleCount =
        qMax(1, static_cast<int>((sourceSpanUs * sampleRate) / drift::kUsPerSecond));

    // Reverse reads the block ahead of the mapped position and flips it below.
    const drift::TimeUs sourceStartUs =
        clip.reverse ? qMax<drift::TimeUs>(0, clip.timelineToSourceUs(playStartUs) - sourceSpanUs)
                     : clip.timelineToSourceUs(playStartUs);

    QVector<float> sourceChunk(sourceSampleCount * 2);
    const int got = ClipReaderPool::instance().readAudioInterleaved(clip.path, sourceStartUs, sourceSampleCount,
                                                                    sampleRate, sourceChunk.data());
    if (got <= 0)
        return out;

    if (clip.reverse && got > 1) {
        for (int i = 0, j = got - 1; i < j; ++i, --j) {
            std::swap(sourceChunk[i * 2], sourceChunk[j * 2]);
            std::swap(sourceChunk[i * 2 + 1], sourceChunk[j * 2 + 1]);
        }
    }

    QVector<float> chunk;
    if (qFuzzyCompare(speed, 1.0))
        chunk = sourceChunk;
    else
        chunk = AudioAtempo::apply(sourceChunk.constData(), got, sampleRate, speed, wantFrames);

    const int copyFrames = qMin(wantFrames, chunk.size() / 2);
    for (int i = 0; i < copyFrames && (leadFrames + i) < outFrames; ++i) {
        out[(leadFrames + i) * 2] = chunk[i * 2];
        out[(leadFrames + i) * 2 + 1] = chunk[i * 2 + 1];
    }
    return out;
}

namespace {

void accumulateClipAudio(const drift::Clip &clip, const drift::Track &track, drift::TimeUs timelineStartUs,
                         int sampleCount, int sampleRate, float *mixBuffer,
                         QHash<QString, std::shared_ptr<AudioEffectChain::Stream>> &effectStreams)
{
    if (clip.path.isEmpty())
        return;

    const drift::TimeUs bufferEndUs = timelineStartUs + static_cast<drift::TimeUs>(
                                                            (static_cast<int64_t>(sampleCount) * drift::kUsPerSecond)
                                                            / sampleRate);

    const bool overlaps = clip.containsTime(timelineStartUs) || clip.containsTime(bufferEndUs - 1)
                          || (timelineStartUs < clip.timelineStart && bufferEndUs > clip.timelineEnd());
    if (!overlaps)
        return;

    const drift::TimeUs blockDurUs = static_cast<drift::TimeUs>(
        (static_cast<int64_t>(sampleCount) * drift::kUsPerSecond) / sampleRate);

    QVector<float> chunk;
    if (!clip.audioEffects.isEmpty()) {
        std::shared_ptr<AudioEffectChain::Stream> &streamPtr = effectStreams[clip.id];
        if (!streamPtr)
            streamPtr = std::make_shared<AudioEffectChain::Stream>();
        AudioEffectChain::Stream &stream = *streamPtr;
        const drift::TimeUs lastEndUs = stream.lastTimelineEndUs();
        const bool continuous = lastEndUs >= 0
                                && qAbs(timelineStartUs - lastEndUs) <= kTimelineGapToleranceUs;
        if (!continuous)
            stream.reset();
        chunk = AudioMixer::readClipAudio(clip, timelineStartUs, sampleCount, sampleRate);
        if (stream.configure(clip.audioEffects, sampleRate)) {
            stream.feed(chunk.constData(), sampleCount);
            chunk = stream.drain(sampleCount);
        }
        stream.setLastTimelineEndUs(timelineStartUs + blockDurUs);
    } else {
        chunk = AudioMixer::readClipAudio(clip, timelineStartUs, sampleCount, sampleRate);
    }

    const int frames = qMin(sampleCount, chunk.size() / 2);
    for (int i = 0; i < frames; ++i) {
        const drift::TimeUs sampleTimeUs =
            timelineStartUs + static_cast<drift::TimeUs>((static_cast<int64_t>(i) * drift::kUsPerSecond) / sampleRate);
        const float gain = static_cast<float>(volumeForClip(clip, sampleTimeUs)
                                              * transitionGainForClip(track, clip, sampleTimeUs)
                                              * clip.fadeMultiplier(sampleTimeUs));
        mixBuffer[i * 2] += chunk[i * 2] * gain;
        mixBuffer[i * 2 + 1] += chunk[i * 2 + 1] * gain;
    }
}

} // namespace

void AudioMixer::setProject(const drift::Project *project)
{
    if (m_project != project)
        m_effectStreams.clear();
    m_project = project;
}

void AudioMixer::resetEffectStreams()
{
    m_effectStreams.clear();
}

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
                accumulateClipAudio(clip, track, timelineStartUs, sampleCount, sampleRate,
                                      interleavedStereoOut, m_effectStreams);
        } else if (track.type == drift::TrackType::Video) {
            for (const drift::Clip &clip : track.clips) {
                if (clip.type == drift::ClipType::Video && !clip.suppressEmbeddedAudio)
                    accumulateClipAudio(clip, track, timelineStartUs, sampleCount, sampleRate,
                                          interleavedStereoOut, m_effectStreams);
            }
        }
    }

    for (int i = 0; i < sampleCount * 2; ++i)
        interleavedStereoOut[i] = softClip(interleavedStereoOut[i]);
}
