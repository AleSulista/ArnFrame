#include "PlaybackEngine.h"

#include <cstring>

namespace {

constexpr int kPlayheadUpdateMs = 16; // ~60 Hz UI updates, independent of video decode

} // namespace

AudioPlaybackIODevice::AudioPlaybackIODevice(PlaybackEngine *engine, QObject *parent)
    : QIODevice(parent)
    , m_engine(engine)
{
    open(QIODevice::ReadOnly);
}

qint64 AudioPlaybackIODevice::readData(char *data, qint64 maxlen)
{
    if (!m_engine || maxlen <= 0)
        return 0;

    const int maxSamples = static_cast<int>(maxlen / (sizeof(float) * 2));
    if (maxSamples <= 0)
        return 0;

    float *buffer = reinterpret_cast<float *>(data);
    const int samples = m_engine->fillAudio(buffer, maxSamples);
    return static_cast<qint64>(samples) * static_cast<qint64>(sizeof(float) * 2);
}

qint64 AudioPlaybackIODevice::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    return -1;
}

qint64 AudioPlaybackIODevice::bytesAvailable() const
{
    return (1 << 20) + QIODevice::bytesAvailable();
}

PlaybackEngine::PlaybackEngine(QObject *parent)
    : QObject(parent)
    , m_device(new AudioPlaybackIODevice(this))
{
    // The pull device and sink run on a dedicated thread; the GUI thread stays
    // free while audio is decoded and mixed on refill.
    m_device->moveToThread(&m_audioThread);
    m_audioThread.setObjectName(QStringLiteral("PlaybackAudio"));
    m_audioThread.start();

    m_playheadTimer.setTimerType(Qt::PreciseTimer);
    m_compositeTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_playheadTimer, &QTimer::timeout, this, &PlaybackEngine::onPlayheadTick);
    connect(&m_compositeTimer, &QTimer::timeout, this, &PlaybackEngine::onCompositeTick);
    connect(&m_compositor, &CompositorService::frameReady, this, &PlaybackEngine::onFrameReady);
}

PlaybackEngine::~PlaybackEngine()
{
    m_playing = false;
    m_playheadTimer.stop();
    m_compositeTimer.stop();
    m_clock.stop();

    // Tear down the sink on its own thread, then stop the thread and reclaim the
    // device (safe once the thread has finished).
    QMetaObject::invokeMethod(
        m_device,
        [this] {
            if (m_sink) {
                m_sink->stop();
                delete m_sink;
                m_sink = nullptr;
            }
        },
        Qt::BlockingQueuedConnection);
    m_audioThread.quit();
    m_audioThread.wait();
    delete m_device;
    m_device = nullptr;
}

void PlaybackEngine::ensureAudioSink()
{
    if (m_project)
        m_sampleRate = m_project->sampleRate();

    QMetaObject::invokeMethod(
        m_device,
        [this] {
            m_format.setSampleRate(m_sampleRate);
            m_format.setChannelCount(2);
            m_format.setSampleFormat(QAudioFormat::Float);

            if (!m_sink) {
                m_sink = new QAudioSink(m_format, m_device);
                // Keep the platform default buffer: a too-small buffer starves the
                // pull loop and makes playback run slow. The playhead is anchored to
                // the sink's played position, so buffer depth doesn't affect sync.
            }
        },
        Qt::BlockingQueuedConnection);
}

void PlaybackEngine::setProject(drift::Project *project)
{
    m_project = project;
    m_mixer.setProject(project);
    m_compositor.setProject(project);
    refreshFrame();
}

void PlaybackEngine::setPlayheadUs(drift::TimeUs us)
{
    m_playheadUs = qMax<drift::TimeUs>(0, us);
    m_clock.reset(m_playheadUs, m_sampleRate);
    if (!m_playing)
        refreshFrame();
}

QImage PlaybackEngine::currentFrame() const
{
    QMutexLocker lock(&m_frameMutex);
    return m_currentFrame;
}

bool PlaybackEngine::hasFrame() const
{
    QMutexLocker lock(&m_frameMutex);
    return !m_currentFrame.isNull() && m_currentFrame.width() > 0;
}

void PlaybackEngine::play()
{
    if (m_playing)
        return;

    ensureAudioSink();
    m_clock.reset(m_playheadUs, m_sampleRate);
    m_clock.start();
    m_playing = true;
    emit playingChanged();

    m_playheadTimer.start(kPlayheadUpdateMs);

    const int fps = m_project ? qMax(1, m_project->fps()) : 30;
    const int tickMs = qMax(1, static_cast<int>(drift::usToSeconds(drift::frameDurationUs(fps)) * 1000.0));
    m_compositeTimer.start(tickMs);

    QMetaObject::invokeMethod(m_device, [this] {
        if (m_sink)
            m_sink->start(m_device);
    });

    onPlayheadTick();
    onCompositeTick();
}

void PlaybackEngine::pause()
{
    if (!m_playing)
        return;

    m_playing = false;
    m_playheadTimer.stop();
    m_compositeTimer.stop();
    m_clock.pause();
    m_playheadUs = m_clock.pausedAt();
    QMetaObject::invokeMethod(
        m_device,
        [this] {
            if (m_sink)
                m_sink->stop();
        },
        Qt::BlockingQueuedConnection);
    emit playingChanged();
    emit playheadUsChanged(static_cast<quint64>(m_playheadUs));
    refreshFrame();
}

void PlaybackEngine::refreshFrame()
{
    m_compositor.requestComposite(m_playheadUs);
}

void PlaybackEngine::checkEndOfTimeline(drift::TimeUs timeUs)
{
    if (!m_project)
        return;

    const drift::TimeUs durationUs = m_project->durationUs();
    if (timeUs >= durationUs) {
        m_playheadUs = durationUs;
        emit playheadUsChanged(static_cast<quint64>(m_playheadUs));
        QMetaObject::invokeMethod(this, &PlaybackEngine::pause, Qt::QueuedConnection);
    }
}

void PlaybackEngine::onPlayheadTick()
{
    if (!m_playing || !m_project)
        return;

    const drift::TimeUs timeUs = m_clock.currentTimeUs();
    if (timeUs == m_playheadUs)
        return;

    m_playheadUs = timeUs;
    emit playheadUsChanged(static_cast<quint64>(timeUs));
    checkEndOfTimeline(timeUs);
}

void PlaybackEngine::onCompositeTick()
{
    if (!m_playing || !m_project)
        return;

    m_compositor.requestComposite(m_clock.currentTimeUs());
}

void PlaybackEngine::onFrameReady(const QImage &frame)
{
    if (frame.isNull())
        return;

    {
        QMutexLocker lock(&m_frameMutex);
        m_currentFrame = frame;
    }
    emit currentFrameChanged();
}

int PlaybackEngine::fillAudio(float *buffer, int sampleCount)
{
    if (!buffer || sampleCount <= 0)
        return 0;

    if (!m_playing || !m_project) {
        std::memset(buffer, 0, static_cast<size_t>(sampleCount) * 2 * sizeof(float));
        return sampleCount;
    }

    // Mix at the produce position (audio we are generating into the buffer),
    // then anchor the visible playhead to what the sink has actually played so
    // video follows audio rather than leading it by the buffer depth.
    const drift::TimeUs timeUs = m_clock.produceTimeUs();
    m_mixer.mix(timeUs, sampleCount, m_sampleRate, buffer);
    m_clock.onAudioSamplesRendered(sampleCount);
    if (m_sink)
        m_clock.syncPlaybackUs(static_cast<drift::TimeUs>(m_sink->processedUSecs()));
    return sampleCount;
}
