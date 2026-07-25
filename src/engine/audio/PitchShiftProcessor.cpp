#include "engine/audio/PitchShiftProcessor.h"

#include <cmath>

namespace drift {

namespace {

constexpr double kTwoPi = 6.283185307179586;
constexpr double kGrainSeconds = 0.050;

} // namespace

void PitchShiftProcessor::setRatio(float ratio)
{
    m_ratio = juce::jlimit(0.25f, 4.0f, ratio);
}

void PitchShiftProcessor::prepare(const juce::dsp::ProcessSpec &spec)
{
    m_sampleRate = spec.sampleRate;
    m_grainSamples = juce::jmax(64, static_cast<int>(spec.sampleRate * kGrainSeconds));
    m_delay.setMaximumDelayInSamples(m_grainSamples + 8);
    m_delay.prepare(spec);
    reset();
}

void PitchShiftProcessor::process(juce::dsp::AudioBlock<float> &block)
{
    const int channels = static_cast<int>(block.getNumChannels());
    const int frames = static_cast<int>(block.getNumSamples());
    const auto grain = static_cast<double>(m_grainSamples);

    // Reading faster than we write raises the pitch, so the read position walks backwards through
    // the line for ratio > 1. One turn of phase is one grain.
    const double increment = (1.0 - static_cast<double>(m_ratio)) / grain;

    for (int i = 0; i < frames; ++i) {
        const double phaseA = m_phase;
        const double phaseB = phaseA < 0.5 ? phaseA + 0.5 : phaseA - 0.5;

        // Complementary raised-cosine windows: they sum to exactly 1, so a steady tone keeps a
        // steady level across the wrap instead of pulsing.
        const auto weightA = static_cast<float>(0.5 * (1.0 - std::cos(kTwoPi * phaseA)));
        const float weightB = 1.0f - weightA;

        const auto delayA = static_cast<float>(juce::jlimit(1.0, grain, phaseA * grain));
        const auto delayB = static_cast<float>(juce::jlimit(1.0, grain, phaseB * grain));

        for (int channel = 0; channel < channels; ++channel) {
            m_delay.pushSample(channel, block.getSample(channel, i));
            const float tapA = m_delay.popSample(channel, delayA, false);
            const float tapB = m_delay.popSample(channel, delayB, false);
            block.setSample(channel, i, tapA * weightA + tapB * weightB);
        }

        m_phase += increment;
        if (m_phase >= 1.0)
            m_phase -= std::floor(m_phase);
        else if (m_phase < 0.0)
            m_phase += std::ceil(-m_phase);
    }
}

void PitchShiftProcessor::reset()
{
    m_delay.reset();
    // Start mid-grain so the first tap already has history behind it.
    m_phase = 0.5;
}

} // namespace drift
