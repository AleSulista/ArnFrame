#pragma once

#include "core/Effect.h"
#include "core/Time.h"

#include <QList>
#include <QString>
#include <QVector>

struct AudioEffectEntry;

struct AVFilterContext;
struct AVFilterGraph;

// Runs a clip's audio effect chain over one mix block through libavfilter.
//
// Stateful filters (echo, chorus, tremolo) need continuous filter state across playback blocks.
// AudioEffectStream keeps one graph alive per clip and only resets on seek or effect changes.
// AudioEffectChain::apply() remains for one-shot / test use.
namespace AudioEffectChain {

QString resolveChain(const AudioEffectEntry &def, const drift::Effect &effect, int sampleRate);

// One-shot pass: prepend preroll, build a fresh graph, return the block. Fine for tests.
QVector<float> apply(const QList<drift::Effect> &effects, const float *interleavedStereo,
                     int prerollFrames, int blockFrames, int sampleRate);

int prerollFramesFor(const QList<drift::Effect> &effects, int sampleRate);

// Persistent filter graph for streaming playback / export blocks.
class Stream
{
public:
    Stream();
    ~Stream();
    Stream(Stream &&other) noexcept;
    Stream &operator=(Stream &&other) noexcept;
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;

    void reset();
    bool configure(const QList<drift::Effect> &effects, int sampleRate);

    void feed(const float *interleavedStereo, int frames);
    // Pull `frames` of interleaved stereo from the filter output. When `discard` is true the
    // samples are dropped (used to skip the preroll warm-up after a seek).
    QVector<float> drain(int frames, bool discard = false);

    drift::TimeUs lastTimelineEndUs() const { return m_lastTimelineEndUs; }
    void setLastTimelineEndUs(drift::TimeUs us) { m_lastTimelineEndUs = us; }

private:
    void destroyGraph();
    bool buildGraph();
    bool pullPending();

    QList<drift::Effect> m_effects;
    int m_sampleRate = 0;
    QString m_signature;
    drift::TimeUs m_lastTimelineEndUs = -1;

    AVFilterGraph *m_graph = nullptr;
    AVFilterContext *m_srcCtx = nullptr;
    AVFilterContext *m_sinkCtx = nullptr;
    QVector<float> m_pending;
};

} // namespace AudioEffectChain
