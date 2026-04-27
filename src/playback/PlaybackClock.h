#pragma once

#include "core/Time.h"

#include <QElapsedTimer>
#include <atomic>

// Audio-master timeline clock with wall-clock fallback when no audio device pulls.
class PlaybackClock
{
public:
    void reset(drift::TimeUs playheadUs, int sampleRate);
    void start();
    void pause();
    void stop();

    bool isRunning() const { return m_running.load(std::memory_order_acquire); }
    drift::TimeUs pausedAt() const { return m_pausedAtUs; }
    drift::TimeUs currentTimeUs() const;

    void onAudioSamplesRendered(int sampleCount);
    void setAudioMaster(bool audio) { m_audioMaster.store(audio, std::memory_order_release); }

private:
    drift::TimeUs m_startPlayheadUs = 0;
    drift::TimeUs m_pausedAtUs = 0;
    std::atomic<int64_t> m_audioSamplesRendered{0};
    std::atomic<bool> m_audioMaster{false};
    std::atomic<bool> m_running{false};
    QElapsedTimer m_wallTimer;
    int m_sampleRate = 48000;
};
