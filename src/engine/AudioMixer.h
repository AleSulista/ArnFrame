#pragma once

#include "core/Project.h"
#include "core/Time.h"
#include "engine/AudioEffectChain.h"

#include <QHash>
#include <memory>

// Mixes active audio clips into interleaved stereo float PCM.
class AudioMixer
{
public:
    void setProject(const drift::Project *project);
    void resetEffectStreams();

    void mix(drift::TimeUs timelineStartUs, int sampleCount, int sampleRate, float *interleavedStereoOut) const;

private:
    const drift::Project *m_project = nullptr;
    mutable QHash<QString, std::shared_ptr<AudioEffectChain::Stream>> m_effectStreams;
};
