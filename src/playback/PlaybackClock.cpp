#include "PlaybackClock.h"

void PlaybackClock::reset(drift::TimeUs playheadUs, int sampleRate)
{
    m_pausedAtUs = qMax<drift::TimeUs>(0, playheadUs);
    m_startPlayheadUs = m_pausedAtUs;
    m_sampleRate = qMax(1, sampleRate);
    m_audioSamplesRendered.store(0, std::memory_order_release);
    m_audioMaster.store(false, std::memory_order_release);
    m_running.store(false, std::memory_order_release);
}

void PlaybackClock::start()
{
    m_startPlayheadUs = m_pausedAtUs;
    m_audioSamplesRendered.store(0, std::memory_order_release);
    m_wallTimer.restart();
    m_running.store(true, std::memory_order_release);
}

void PlaybackClock::pause()
{
    if (!m_running.load(std::memory_order_acquire))
        return;

    m_pausedAtUs = currentTimeUs();
    m_running.store(false, std::memory_order_release);
}

void PlaybackClock::stop()
{
    pause();
    m_audioSamplesRendered.store(0, std::memory_order_release);
    m_audioMaster.store(false, std::memory_order_release);
}

drift::TimeUs PlaybackClock::currentTimeUs() const
{
    if (!m_running.load(std::memory_order_acquire))
        return m_pausedAtUs;

    if (m_audioMaster.load(std::memory_order_acquire)) {
        const int64_t samples = m_audioSamplesRendered.load(std::memory_order_acquire);
        if (samples > 0) {
            return m_startPlayheadUs
                   + static_cast<drift::TimeUs>((samples * drift::kUsPerSecond) / m_sampleRate);
        }
    }

    return m_startPlayheadUs + static_cast<drift::TimeUs>(m_wallTimer.elapsed()) * drift::kUsPerMs;
}

void PlaybackClock::onAudioSamplesRendered(int sampleCount)
{
    if (sampleCount <= 0)
        return;

    m_audioSamplesRendered.fetch_add(sampleCount, std::memory_order_acq_rel);
    m_audioMaster.store(true, std::memory_order_release);
}
