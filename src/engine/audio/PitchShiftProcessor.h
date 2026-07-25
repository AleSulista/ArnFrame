#pragma once

#include "engine/audio/AudioEffectProcessor.h"
#include "engine/audio/ModulationProcessors.h"

namespace drift {

// Granular pitch shift: two taps half a grain apart read out of a delay line at a rate set by the
// pitch ratio, crossfaded so the wrap point is inaudible. Duration is preserved, which is what the
// asetrate + aresample + atempo chain this replaces was doing the long way round.
//
// juce_dsp has no pitch shifter and none was vendored, so this is the one stage with no library
// primitive behind it. Transients smear a little; for cartoon voice effects that is the accepted
// trade, and it beats the old chain's per-block atempo state reset.
class PitchShiftProcessor final : public AudioEffectProcessor
{
public:
    void setRatio(float ratio);

    void prepare(const juce::dsp::ProcessSpec &spec) override;
    void process(juce::dsp::AudioBlock<float> &block) override;
    void reset() override;

    // The taps sweep across a whole grain, so the average read position sits half a grain back.
    int latencySamples() const override { return m_grainSamples / 2; }

private:
    LinearDelayLine m_delay{16384};
    double m_sampleRate = 48000.0;
    double m_phase = 0.0;
    int m_grainSamples = 2400;
    float m_ratio = 1.0f;
};

} // namespace drift
