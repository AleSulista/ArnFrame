#include "PlaybackClock.h"

qint64 PlaybackClock::nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void PlaybackClock::reset(drift::TimeUs playheadUs, int sampleRate)
{
    m_pausedAtUs = qMax<drift::TimeUs>(0, playheadUs);
    m_startPlayheadUs = m_pausedAtUs;
    m_sampleRate = qMax(1, sampleRate);
    m_audioSamplesRendered.store(0, std::memory_order_release);
    m_running.store(false, std::memory_order_release);

    QMutexLocker lock(&m_anchorMutex);
    m_anchorPlayedUs = 0;
    m_anchorWallNs = 0;
}

void PlaybackClock::start()
{
    m_startPlayheadUs = m_pausedAtUs;
    m_audioSamplesRendered.store(0, std::memory_order_release);
    {
        QMutexLocker lock(&m_anchorMutex);
        m_anchorPlayedUs = 0;
        m_anchorWallNs = 0;
        m_startWallNs = nowNs();
    }
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
}

drift::TimeUs PlaybackClock::produceTimeUs() const
{
    if (!m_running.load(std::memory_order_acquire))
        return m_pausedAtUs;

    const int64_t samples = m_audioSamplesRendered.load(std::memory_order_acquire);
    return m_startPlayheadUs + static_cast<drift::TimeUs>((samples * drift::kUsPerSecond) / m_sampleRate);
}

void PlaybackClock::onAudioSamplesRendered(int sampleCount)
{
    if (sampleCount <= 0)
        return;
    m_audioSamplesRendered.fetch_add(sampleCount, std::memory_order_acq_rel);
}

void PlaybackClock::syncPlaybackUs(drift::TimeUs playedUs)
{
    QMutexLocker lock(&m_anchorMutex);
    m_anchorPlayedUs = qMax<drift::TimeUs>(0, playedUs);
    m_anchorWallNs = nowNs();
}

drift::TimeUs PlaybackClock::currentTimeUs() const
{
    if (!m_running.load(std::memory_order_acquire))
        return m_pausedAtUs;

    QMutexLocker lock(&m_anchorMutex);
    if (m_anchorWallNs == 0) // no sink sync yet; advance on wall time from start
        return m_startPlayheadUs + qMax<drift::TimeUs>(0, (nowNs() - m_startWallNs) / 1000);

    const drift::TimeUs interpUs = (nowNs() - m_anchorWallNs) / 1000;
    return m_startPlayheadUs + m_anchorPlayedUs + qMax<drift::TimeUs>(0, interpUs);
}
