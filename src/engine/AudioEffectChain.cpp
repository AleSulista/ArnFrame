#include "AudioEffectChain.h"

#include "AudioEffectCatalog.h"

#include <QByteArray>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

namespace {

QString formatValue(const QVariant &value)
{
    bool ok = false;
    const double d = value.toDouble(&ok);
    if (!ok)
        return value.toString();
    if (std::floor(d) == d && std::abs(d) < 1e15)
        return QString::number(static_cast<long long>(d));
    return QString::number(d, 'g', 10);
}

QString effectSignature(const QList<drift::Effect> &effects, int sampleRate)
{
    QString sig = QString::number(sampleRate);
    for (const drift::Effect &effect : effects) {
        sig += QLatin1Char('|');
        sig += effect.catalogId;
        for (auto it = effect.parameters.constBegin(); it != effect.parameters.constEnd(); ++it)
            sig += it.key() + QLatin1Char('=') + formatValue(it.value());
    }
    return sig;
}

QString buildCombinedChain(const QList<drift::Effect> &effects, int sampleRate)
{
    QStringList parts;
    for (const drift::Effect &effect : effects) {
        const AudioEffectEntry *def = audioEffectDefForId(effect.catalogId);
        if (!def)
            continue;
        parts.append(AudioEffectChain::resolveChain(*def, effect, sampleRate));
    }
    return parts.join(QLatin1Char(','));
}

bool createGraph(const QString &chain, int sampleRate, AVFilterGraph **graphOut,
                 AVFilterContext **srcOut, AVFilterContext **sinkOut)
{
    if (chain.isEmpty() || sampleRate <= 0)
        return false;

    AVFilterGraph *graph = avfilter_graph_alloc();
    if (!graph)
        return false;

    const AVFilter *abuffer = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    if (!abuffer || !abuffersink) {
        avfilter_graph_free(&graph);
        return false;
    }

    AVFilterContext *srcCtx = nullptr;
    AVFilterContext *sinkCtx = nullptr;
    const QByteArray srcArgs =
        QStringLiteral("time_base=1/%1:sample_rate=%1:sample_fmt=flt:channel_layout=stereo")
            .arg(sampleRate)
            .toUtf8();
    if (avfilter_graph_create_filter(&srcCtx, abuffer, "in", srcArgs.constData(), nullptr, graph) < 0
        || avfilter_graph_create_filter(&sinkCtx, abuffersink, "out", nullptr, nullptr, graph) < 0) {
        avfilter_graph_free(&graph);
        return false;
    }

    const QByteArray graphDesc =
        QStringLiteral("[in] %1,aformat=sample_fmts=flt:channel_layouts=stereo [out]").arg(chain).toUtf8();
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    outputs->name = av_strdup("in");
    outputs->filter_ctx = srcCtx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = sinkCtx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    if (avfilter_graph_parse_ptr(graph, graphDesc.constData(), &inputs, &outputs, nullptr) < 0
        || avfilter_graph_config(graph, nullptr) < 0) {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&graph);
        return false;
    }
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    *graphOut = graph;
    *srcOut = srcCtx;
    *sinkOut = sinkCtx;
    return true;
}

QVector<float> runGraphOnce(const QString &chain, const float *interleavedStereo, int inFrames,
                            int outFrames, int sampleRate)
{
    QVector<float> result;
    if (chain.isEmpty() || !interleavedStereo || inFrames <= 0 || sampleRate <= 0)
        return result;

    AVFilterGraph *graph = nullptr;
    AVFilterContext *srcCtx = nullptr;
    AVFilterContext *sinkCtx = nullptr;
    if (!createGraph(chain, sampleRate, &graph, &srcCtx, &sinkCtx))
        return result;

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        avfilter_graph_free(&graph);
        return result;
    }
    frame->format = AV_SAMPLE_FMT_FLT;
    frame->sample_rate = sampleRate;
    av_channel_layout_default(&frame->ch_layout, 2);
    frame->nb_samples = inFrames;
    if (av_frame_get_buffer(frame, 0) < 0) {
        av_frame_free(&frame);
        avfilter_graph_free(&graph);
        return result;
    }
    std::memcpy(frame->data[0], interleavedStereo, static_cast<size_t>(inFrames) * 2 * sizeof(float));
    frame->pts = 0;

    if (av_buffersrc_add_frame_flags(srcCtx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
        av_frame_free(&frame);
        avfilter_graph_free(&graph);
        return result;
    }
    av_frame_free(&frame);
    if (av_buffersrc_add_frame_flags(srcCtx, nullptr, 0) < 0) { /* tail unavailable */
    }

    QVector<float> collected;
    collected.reserve(inFrames * 2);
    while (true) {
        AVFrame *outFrame = av_frame_alloc();
        const int ret = av_buffersink_get_frame(sinkCtx, outFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&outFrame);
            break;
        }
        if (ret < 0) {
            av_frame_free(&outFrame);
            break;
        }
        const int channels = outFrame->ch_layout.nb_channels;
        const float *data = reinterpret_cast<const float *>(outFrame->data[0]);
        for (int i = 0; i < outFrame->nb_samples; ++i) {
            collected.append(data[i * channels]);
            collected.append(channels > 1 ? data[i * channels + 1] : data[i * channels]);
        }
        av_frame_free(&outFrame);
    }
    avfilter_graph_free(&graph);

    const int gotFrames = collected.size() / 2;
    if (gotFrames <= 0)
        return result;

    result.resize(outFrames * 2);
    result.fill(0.0f);
    const int start = std::max(0, gotFrames - outFrames);
    const int copyFrames = std::min(outFrames, gotFrames - start);
    for (int i = 0; i < copyFrames; ++i) {
        result[i * 2] = collected[(start + i) * 2];
        result[i * 2 + 1] = collected[(start + i) * 2 + 1];
    }
    return result;
}

} // namespace

QString AudioEffectChain::resolveChain(const AudioEffectEntry &def, const drift::Effect &effect,
                                       int sampleRate)
{
    QString chain = def.chainTemplate;
    chain.replace(QStringLiteral("{sampleRate}"), QString::number(sampleRate));
    const QMap<QString, QVariant> params = resolvedAudioEffectParameters(effect, def);
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        chain.replace(QStringLiteral("{%1}").arg(it.key()), formatValue(it.value()));
    return chain;
}

int AudioEffectChain::prerollFramesFor(const QList<drift::Effect> &effects, int sampleRate)
{
    int maxMs = 0;
    for (const drift::Effect &effect : effects) {
        const AudioEffectEntry *def = audioEffectDefForId(effect.catalogId);
        if (def)
            maxMs = std::max(maxMs, def->prerollMs);
    }
    return static_cast<int>((static_cast<int64_t>(maxMs) * sampleRate) / 1000);
}

QVector<float> AudioEffectChain::apply(const QList<drift::Effect> &effects,
                                       const float *interleavedStereo, int prerollFrames,
                                       int blockFrames, int sampleRate)
{
    const int totalFrames = prerollFrames + blockFrames;

    QVector<float> current(totalFrames * 2);
    if (interleavedStereo && totalFrames > 0)
        std::memcpy(current.data(), interleavedStereo, static_cast<size_t>(totalFrames) * 2 * sizeof(float));

    int currentPreroll = prerollFrames;
    for (const drift::Effect &effect : effects) {
        const AudioEffectEntry *def = audioEffectDefForId(effect.catalogId);
        if (!def)
            continue;
        const QString chain = resolveChain(*def, effect, sampleRate);
        const int inFrames = currentPreroll + blockFrames;
        const QVector<float> out =
            runGraphOnce(chain, current.constData(), inFrames, currentPreroll + blockFrames, sampleRate);
        if (out.isEmpty())
            continue;
        current = out;
    }

    QVector<float> block(blockFrames * 2, 0.0f);
    const int available = current.size() / 2;
    const int start = std::min(currentPreroll, std::max(0, available - blockFrames));
    const int copyFrames = std::min(blockFrames, available - start);
    for (int i = 0; i < copyFrames; ++i) {
        block[i * 2] = current[(start + i) * 2];
        block[i * 2 + 1] = current[(start + i) * 2 + 1];
    }
    return block;
}

AudioEffectChain::Stream::Stream() = default;

AudioEffectChain::Stream::~Stream()
{
    destroyGraph();
}

AudioEffectChain::Stream::Stream(Stream &&other) noexcept
    : m_effects(std::move(other.m_effects))
    , m_sampleRate(other.m_sampleRate)
    , m_signature(std::move(other.m_signature))
    , m_lastTimelineEndUs(other.m_lastTimelineEndUs)
    , m_graph(other.m_graph)
    , m_srcCtx(other.m_srcCtx)
    , m_sinkCtx(other.m_sinkCtx)
    , m_pending(std::move(other.m_pending))
{
    other.m_graph = nullptr;
    other.m_srcCtx = nullptr;
    other.m_sinkCtx = nullptr;
    other.m_sampleRate = 0;
    other.m_lastTimelineEndUs = -1;
}

AudioEffectChain::Stream &AudioEffectChain::Stream::operator=(Stream &&other) noexcept
{
    if (this != &other) {
        destroyGraph();
        m_effects = std::move(other.m_effects);
        m_sampleRate = other.m_sampleRate;
        m_signature = std::move(other.m_signature);
        m_lastTimelineEndUs = other.m_lastTimelineEndUs;
        m_graph = other.m_graph;
        m_srcCtx = other.m_srcCtx;
        m_sinkCtx = other.m_sinkCtx;
        m_pending = std::move(other.m_pending);
        other.m_graph = nullptr;
        other.m_srcCtx = nullptr;
        other.m_sinkCtx = nullptr;
        other.m_sampleRate = 0;
        other.m_lastTimelineEndUs = -1;
    }
    return *this;
}

void AudioEffectChain::Stream::destroyGraph()
{
    if (m_graph)
        avfilter_graph_free(&m_graph);
    m_graph = nullptr;
    m_srcCtx = nullptr;
    m_sinkCtx = nullptr;
}

void AudioEffectChain::Stream::reset()
{
    destroyGraph();
    m_pending.clear();
    m_lastTimelineEndUs = -1;
}

bool AudioEffectChain::Stream::buildGraph()
{
    destroyGraph();
    const QString chain = buildCombinedChain(m_effects, m_sampleRate);
    return createGraph(chain, m_sampleRate, &m_graph, &m_srcCtx, &m_sinkCtx);
}

bool AudioEffectChain::Stream::configure(const QList<drift::Effect> &effects, int sampleRate)
{
    const QString sig = effectSignature(effects, sampleRate);
    if (sig == m_signature && m_graph)
        return true;

    reset();
    m_effects = effects;
    m_sampleRate = sampleRate;
    m_signature = sig;
    return buildGraph();
}

bool AudioEffectChain::Stream::pullPending()
{
    if (!m_sinkCtx)
        return false;

    AVFrame *outFrame = av_frame_alloc();
    if (!outFrame)
        return false;

    const int ret = av_buffersink_get_frame(m_sinkCtx, outFrame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_free(&outFrame);
        return false;
    }
    if (ret < 0) {
        av_frame_free(&outFrame);
        return false;
    }

    const int channels = outFrame->ch_layout.nb_channels;
    const float *data = reinterpret_cast<const float *>(outFrame->data[0]);
    for (int i = 0; i < outFrame->nb_samples; ++i) {
        m_pending.append(data[i * channels]);
        m_pending.append(channels > 1 ? data[i * channels + 1] : data[i * channels]);
    }
    av_frame_free(&outFrame);
    return true;
}

void AudioEffectChain::Stream::feed(const float *interleavedStereo, int frames)
{
    if (!m_srcCtx || !interleavedStereo || frames <= 0)
        return;

    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return;

    frame->format = AV_SAMPLE_FMT_FLT;
    frame->sample_rate = m_sampleRate;
    av_channel_layout_default(&frame->ch_layout, 2);
    frame->nb_samples = frames;
    if (av_frame_get_buffer(frame, 0) < 0) {
        av_frame_free(&frame);
        return;
    }
    std::memcpy(frame->data[0], interleavedStereo, static_cast<size_t>(frames) * 2 * sizeof(float));
    frame->pts = 0;
    av_buffersrc_add_frame_flags(m_srcCtx, frame, 0);
    av_frame_free(&frame);
}

QVector<float> AudioEffectChain::Stream::drain(int frames, bool discard)
{
    QVector<float> out;
    if (frames <= 0)
        return out;

    int pulls = 0;
    const int maxPulls = frames * 4 + 64;
    while (m_pending.size() / 2 < frames && pulls < maxPulls) {
        const int before = m_pending.size();
        if (!pullPending() || m_pending.size() == before)
            break;
        ++pulls;
    }

    const int available = m_pending.size() / 2;
    const int take = std::min(frames, available);
    if (discard) {
        m_pending.remove(0, take * 2);
        return out;
    }

    out.resize(frames * 2);
    out.fill(0.0f);
    for (int i = 0; i < take; ++i) {
        out[i * 2] = m_pending[i * 2];
        out[i * 2 + 1] = m_pending[i * 2 + 1];
    }
    m_pending.remove(0, take * 2);
    return out;
}
