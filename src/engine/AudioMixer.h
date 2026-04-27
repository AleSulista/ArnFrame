#pragma once

#include "core/Project.h"
#include "core/Time.h"

// Mixes active audio clips into interleaved stereo float PCM.
class AudioMixer
{
public:
    void setProject(const drift::Project *project) { m_project = project; }

    void mix(drift::TimeUs timelineStartUs, int sampleCount, int sampleRate, float *interleavedStereoOut) const;

private:
    const drift::Project *m_project = nullptr;
};
