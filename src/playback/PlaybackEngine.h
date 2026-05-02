#pragma once

#include "CompositorService.h"
#include "PlaybackClock.h"
#include "core/Project.h"
#include "core/Time.h"
#include "engine/AudioMixer.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QTimer>

#include <atomic>

class PlaybackEngine;

// Pull-mode audio device; rendered samples advance the audio-master clock.
class AudioPlaybackIODevice : public QIODevice
{
public:
    explicit AudioPlaybackIODevice(PlaybackEngine *engine, QObject *parent = nullptr);

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    // Endless generated source: always advertise data so the sink keeps pulling.
    qint64 bytesAvailable() const override;
    bool isSequential() const override { return true; }

private:
    PlaybackEngine *m_engine = nullptr;
};

// Audio-master playback: mixes timeline audio and composites preview frames off the GUI thread.
class PlaybackEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QImage currentFrame READ currentFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)

public:
    explicit PlaybackEngine(QObject *parent = nullptr);
    ~PlaybackEngine() override;

    void setProject(drift::Project *project);
    void setPlayheadUs(drift::TimeUs us);
    drift::TimeUs playheadUs() const { return m_playheadUs; }

    QImage currentFrame() const;
    bool hasFrame() const;
    bool isPlaying() const { return m_playing; }

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void refreshFrame();

signals:
    void currentFrameChanged();
    void playingChanged();
    void playheadUsChanged(quint64 us);

private:
    friend class AudioPlaybackIODevice;

    int fillAudio(float *buffer, int sampleCount);
    void ensureAudioSink();
    void onPlayheadTick();
    void onCompositeTick();
    void onFrameReady(const QImage &frame);
    void checkEndOfTimeline(drift::TimeUs timeUs);

    drift::Project *m_project = nullptr;
    PlaybackClock m_clock;
    CompositorService m_compositor;
    AudioMixer m_mixer;
    QAudioFormat m_format;
    // The sink and its pull device live on m_audioThread so that ring-buffer
    // refills (which synchronously decode audio) never block the GUI thread.
    QThread m_audioThread;
    QAudioSink *m_sink = nullptr;
    AudioPlaybackIODevice *m_device = nullptr;
    QTimer m_playheadTimer;
    QTimer m_compositeTimer;
    QImage m_currentFrame;
    mutable QMutex m_frameMutex;
    drift::TimeUs m_playheadUs = 0;
    std::atomic<bool> m_playing = false;
    int m_sampleRate = 48000;
};
