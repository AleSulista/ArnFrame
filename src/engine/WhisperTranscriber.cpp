#include "WhisperTranscriber.h"

#include "WhisperTokenizer.h"
#include "GpuPackageParse.h"
#include "core/Time.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <onnxruntime_cxx_api.h>

extern "C" {
#include <libavutil/mem.h>
#include <libavutil/tx.h>
}

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace drift {

namespace {

// Whisper-small feature-extractor constants.
constexpr int kSampleRate = 16000;
constexpr int kNFft = 400;
constexpr int kHop = 160;
constexpr int kNMel = 80;
constexpr int kNBins = kNFft / 2 + 1; // 201
constexpr int kChunkSamples = 30 * kSampleRate; // 480000
constexpr int kNFrames = 3000;

// Token ids (from generation_config.json).
constexpr int kVocab = 51865;
constexpr int kSotToken = 50258;        // <|startoftranscript|>
constexpr int kEosToken = 50257;        // <|endoftext|>
constexpr int kTranscribeToken = 50359; // <|transcribe|>
constexpr int kNoTimestampsToken = 50363;
constexpr int kTimestampBegin = 50364; // <|0.00|>
constexpr int kMaxDecodeTokens = 224;  // per 30s window

std::basic_string<ORTCHAR_T> ortPath(const QString &path)
{
#ifdef _WIN32
    return path.toStdWString();
#else
    return path.toStdString();
#endif
}

double hertzToMel(double freq)
{
    constexpr double minLogHertz = 1000.0;
    constexpr double minLogMel = 15.0;
    const double logstep = 27.0 / std::log(6.4);
    if (freq >= minLogHertz)
        return minLogMel + std::log(freq / minLogHertz) * logstep;
    return 3.0 * freq / 200.0;
}

double melToHertz(double mel)
{
    constexpr double minLogHertz = 1000.0;
    constexpr double minLogMel = 15.0;
    const double logstep = std::log(6.4) / 27.0;
    if (mel >= minLogMel)
        return minLogHertz * std::exp(logstep * (mel - minLogMel));
    return 200.0 * mel / 3.0;
}

// Slaney-normalized mel filterbank matching transformers.WhisperFeatureExtractor.
// Returns filters[kNMel][kNBins].
std::vector<std::vector<float>> buildMelFilters()
{
    std::vector<double> filterFreqs(kNMel + 2);
    const double melMin = hertzToMel(0.0);
    const double melMax = hertzToMel(8000.0);
    for (int i = 0; i < kNMel + 2; ++i) {
        const double mel = melMin + (melMax - melMin) * i / (kNMel + 1);
        filterFreqs[i] = melToHertz(mel);
    }

    std::vector<double> fftFreqs(kNBins);
    for (int k = 0; k < kNBins; ++k)
        fftFreqs[k] = static_cast<double>(k) * (kSampleRate / 2.0) / (kNBins - 1);

    std::vector<std::vector<float>> filters(kNMel, std::vector<float>(kNBins, 0.0f));
    for (int m = 0; m < kNMel; ++m) {
        const double left = filterFreqs[m];
        const double center = filterFreqs[m + 1];
        const double right = filterFreqs[m + 2];
        const double leftDiff = center - left;
        const double rightDiff = right - center;
        const double enorm = 2.0 / (right - left); // slaney norm
        for (int k = 0; k < kNBins; ++k) {
            const double down = (fftFreqs[k] - left) / leftDiff;
            const double up = (right - fftFreqs[k]) / rightDiff;
            const double v = std::min(down, up);
            filters[m][k] = static_cast<float>(std::max(0.0, v) * enorm);
        }
    }
    return filters;
}

} // namespace

struct WhisperTranscriber::Impl
{
    bool loaded = false;
    bool loadAttempted = false;
    QString error;
    QString modelDir;

    Ort::Env env{ORT_LOGGING_LEVEL_ERROR, "drift-whisper"};
    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::unique_ptr<Ort::Session> encoder;
    std::unique_ptr<Ort::Session> decoder;
    std::unique_ptr<Ort::Session> decoderPast;

    std::vector<std::string> encInNames, encOutNames;
    std::vector<std::string> decInNames, decOutNames;
    std::vector<std::string> decpInNames, decpOutNames;

    WhisperTokenizer tokenizer;
    std::vector<std::vector<float>> melFilters;
    std::vector<float> hann;

    std::vector<char> suppressMask;      // vocab-sized, from suppress_tokens (+ no_timestamps)
    std::vector<int> beginSuppress;      // begin_suppress_tokens
    std::vector<int> languageTokenIds;   // lang_to_id values

    // Scratch FFT buffers (av_tx).
    AVTXContext *tx = nullptr;
    av_tx_fn txFn = nullptr;
    float *fftIn = nullptr;
    void *fftOut = nullptr; // AVComplexFloat[kNBins]

    ~Impl()
    {
        if (tx)
            av_tx_uninit(&tx);
        av_free(fftIn);
        av_free(fftOut);
    }

    bool ensureLoaded();
    std::vector<std::string> names(Ort::Session &s, bool inputs);
    std::vector<float> logMel(const float *pcm, int count); // returns [kNMel*kNFrames]
    Ort::Value runEncoder(const std::vector<float> &mel);
    int argmax(const float *logits, int minTimestamp, bool firstStep) const;
};

int WhisperTranscriber::Impl::argmax(const float *logits, int minTimestamp, bool firstStep) const
{
    int best = kEosToken;
    float bestVal = -1e30f;
    for (int v = 0; v < kVocab; ++v) {
        if (suppressMask[v])
            continue;
        if (v >= kTimestampBegin && v < minTimestamp)
            continue; // timestamps must not go backward
        if (firstStep && std::find(beginSuppress.begin(), beginSuppress.end(), v)
                != beginSuppress.end())
            continue;
        if (logits[v] > bestVal) {
            bestVal = logits[v];
            best = v;
        }
    }
    return best;
}

std::vector<std::string> WhisperTranscriber::Impl::names(Ort::Session &s, bool inputs)
{
    Ort::AllocatorWithDefaultOptions alloc;
    std::vector<std::string> out;
    const size_t n = inputs ? s.GetInputCount() : s.GetOutputCount();
    for (size_t i = 0; i < n; ++i) {
        auto name = inputs ? s.GetInputNameAllocated(i, alloc) : s.GetOutputNameAllocated(i, alloc);
        out.emplace_back(name.get());
    }
    return out;
}

bool WhisperTranscriber::Impl::ensureLoaded()
{
    if (loadAttempted)
        return loaded;
    loadAttempted = true;

    const QStringList roots =
        GpuPackageParse::defaultSearchPaths(QStringLiteral("DRIFT_WHISPER_MODEL_DIR"),
                                            QStringLiteral("models/whisper-small"));
    for (const QString &root : roots) {
        if (QFile::exists(QDir(root).filePath(QStringLiteral("encoder_model_fp16.onnx")))) {
            modelDir = root;
            break;
        }
    }
    if (modelDir.isEmpty()) {
        error = QStringLiteral("Whisper model not found. Place it in models/whisper-small "
                               "or set DRIFT_WHISPER_MODEL_DIR.");
        return false;
    }

    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(std::max(1, QThread::idealThreadCount()));
        // fp16 graph fusions (SimplifiedLayerNormFusion) crash on load; disable them.
        opts.SetGraphOptimizationLevel(ORT_DISABLE_ALL);

        const QDir dir(modelDir);
        encoder = std::make_unique<Ort::Session>(
            env, ortPath(dir.filePath(QStringLiteral("encoder_model_fp16.onnx"))).c_str(), opts);
        decoder = std::make_unique<Ort::Session>(
            env, ortPath(dir.filePath(QStringLiteral("decoder_model_fp16.onnx"))).c_str(), opts);
        decoderPast = std::make_unique<Ort::Session>(
            env, ortPath(dir.filePath(QStringLiteral("decoder_with_past_model_fp16.onnx"))).c_str(),
            opts);

        encInNames = names(*encoder, true);
        encOutNames = names(*encoder, false);
        decInNames = names(*decoder, true);
        decOutNames = names(*decoder, false);
        decpInNames = names(*decoderPast, true);
        decpOutNames = names(*decoderPast, false);
    } catch (const Ort::Exception &e) {
        error = QStringLiteral("Failed to load Whisper model: ") + QString::fromUtf8(e.what());
        return false;
    }

    if (!tokenizer.load(QDir(modelDir).filePath(QStringLiteral("vocab.json")))) {
        error = QStringLiteral("Failed to load Whisper tokenizer (vocab.json).");
        return false;
    }

    // Parse generation_config.json for suppression + language tokens.
    suppressMask.assign(kVocab, 0);
    suppressMask[kNoTimestampsToken] = 1; // always emit timestamps
    QFile gc(QDir(modelDir).filePath(QStringLiteral("generation_config.json")));
    if (gc.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(gc.readAll()).object();
        for (const QJsonValue v : obj.value(QStringLiteral("suppress_tokens")).toArray()) {
            const int t = v.toInt(-1);
            if (t >= 0 && t < kVocab)
                suppressMask[t] = 1;
        }
        for (const QJsonValue v : obj.value(QStringLiteral("begin_suppress_tokens")).toArray()) {
            const int t = v.toInt(-1);
            if (t >= 0 && t < kVocab)
                beginSuppress.push_back(t);
        }
        const QJsonObject langs = obj.value(QStringLiteral("lang_to_id")).toObject();
        for (auto it = langs.constBegin(); it != langs.constEnd(); ++it)
            languageTokenIds.push_back(it.value().toInt());
    }

    melFilters = buildMelFilters();
    hann.resize(kNFft);
    for (int n = 0; n < kNFft; ++n)
        hann[n] = 0.5f * (1.0f - std::cos(2.0 * M_PI * n / kNFft)); // periodic Hann

    float scale = 1.0f;
    fftIn = static_cast<float *>(av_malloc(sizeof(float) * kNFft));
    fftOut = av_malloc(sizeof(float) * 2 * (kNBins + 1));
    if (av_tx_init(&tx, &txFn, AV_TX_FLOAT_RDFT, 0, kNFft, &scale, 0) < 0) {
        error = QStringLiteral("Failed to initialize FFT (av_tx).");
        return false;
    }

    loaded = true;
    return true;
}

std::vector<float> WhisperTranscriber::Impl::logMel(const float *pcm, int count)
{
    const int pad = kNFft / 2;
    std::vector<float> padded(kChunkSamples + kNFft, 0.0f);
    const int n = std::min(count, kChunkSamples);
    for (int i = 0; i < n; ++i)
        padded[pad + i] = pcm[i];
    // Reflect padding (numpy 'reflect', edge sample not repeated).
    for (int k = 1; k <= pad && k < n; ++k)
        padded[pad - k] = pcm[k];
    for (int k = 1; k <= pad && k < n; ++k)
        padded[pad + n - 1 + k] = pcm[n - 1 - k];

    auto *cout = static_cast<float *>(fftOut); // interleaved re,im
    std::vector<float> mel(kNMel * kNFrames, 0.0f);
    float logMax = -1e30f;

    for (int t = 0; t < kNFrames; ++t) {
        const int start = t * kHop;
        for (int i = 0; i < kNFft; ++i)
            fftIn[i] = padded[start + i] * hann[i];
        txFn(tx, cout, fftIn, sizeof(float));

        for (int m = 0; m < kNMel; ++m) {
            const std::vector<float> &fil = melFilters[m];
            float acc = 0.0f;
            for (int k = 0; k < kNBins; ++k) {
                const float re = cout[2 * k];
                const float im = cout[2 * k + 1];
                acc += fil[k] * (re * re + im * im);
            }
            float v = std::log10(std::max(acc, 1e-10f));
            mel[m * kNFrames + t] = v;
            logMax = std::max(logMax, v);
        }
    }

    const float floor = logMax - 8.0f;
    for (float &v : mel)
        v = (std::max(v, floor) + 4.0f) / 4.0f;
    return mel;
}

Ort::Value WhisperTranscriber::Impl::runEncoder(const std::vector<float> &mel)
{
    const int64_t shape[3] = {1, kNMel, kNFrames};
    Ort::Value in = Ort::Value::CreateTensor<float>(mem, const_cast<float *>(mel.data()),
                                                    mel.size(), shape, 3);
    const char *inName = encInNames[0].c_str();
    const char *outName = encOutNames[0].c_str();
    auto outs = encoder->Run(Ort::RunOptions{nullptr}, &inName, &in, 1, &outName, 1);
    return std::move(outs[0]);
}

// --- WhisperTranscriber ----------------------------------------------------

WhisperTranscriber::WhisperTranscriber() : d(std::make_unique<Impl>()) {}
WhisperTranscriber::~WhisperTranscriber() = default;

WhisperTranscriber &WhisperTranscriber::instance()
{
    static WhisperTranscriber s;
    return s;
}

bool WhisperTranscriber::available()
{
    return d->ensureLoaded();
}

QString WhisperTranscriber::lastError() const
{
    return d->error;
}

namespace {

Ort::Value floatView(const Ort::Value &src, const Ort::MemoryInfo &mem)
{
    auto info = src.GetTensorTypeAndShapeInfo();
    auto shape = info.GetShape();
    return Ort::Value::CreateTensor<float>(mem, const_cast<float *>(src.GetTensorData<float>()),
                                           info.GetElementCount(), shape.data(), shape.size());
}

std::string presentToPast(const std::string &name)
{
    // "present.3.decoder.key" -> "past_key_values.3.decoder.key"
    const std::string prefix = "present";
    if (name.rfind(prefix, 0) == 0)
        return "past_key_values" + name.substr(prefix.size());
    return name;
}

} // namespace

WhisperResult WhisperTranscriber::transcribe(const std::vector<float> &pcm,
                                             const std::function<bool(double)> &progress)
{
    WhisperResult result;
    if (!d->ensureLoaded()) {
        result.error = d->error;
        return result;
    }

    const int total = static_cast<int>(pcm.size());
    if (total <= 0) {
        result.ok = true;
        return result;
    }
    const double totalSeconds = static_cast<double>(total) / kSampleRate;

    // Detect language once from the first window.
    int cursor = 0;
    int languageToken = 50259; // <|en|> fallback

    while (cursor < total) {
        if (progress && !progress(std::min(1.0, static_cast<double>(cursor) / total))) {
            result.cancelled = true;
            return result;
        }

        const double windowStartSec = static_cast<double>(cursor) / kSampleRate;
        const std::vector<float> mel = d->logMel(pcm.data() + cursor, total - cursor);

        Ort::Value encHidden = d->runEncoder(mel);

        // Language detection on the first window only.
        if (cursor == 0) {
            std::vector<int64_t> ids{kSotToken};
            const int64_t idShape[2] = {1, 1};
            Ort::Value idTensor =
                Ort::Value::CreateTensor<int64_t>(d->mem, ids.data(), ids.size(), idShape, 2);
            Ort::Value encView = floatView(encHidden, d->mem);
            std::vector<Ort::Value> ins;
            ins.push_back(std::move(idTensor));
            ins.push_back(std::move(encView));
            std::vector<const char *> inN{d->decInNames[0].c_str(), d->decInNames[1].c_str()};
            std::vector<const char *> outN;
            for (const auto &n : d->decOutNames)
                outN.push_back(n.c_str());
            auto outs = d->decoder->Run(Ort::RunOptions{nullptr}, inN.data(), ins.data(), ins.size(),
                                        outN.data(), outN.size());
            const float *logits = outs[0].GetTensorMutableData<float>();
            float best = -1e30f;
            for (const int lang : d->languageTokenIds) {
                if (lang >= 0 && lang < kVocab && logits[lang] > best) {
                    best = logits[lang];
                    languageToken = lang;
                }
            }
        }

        // Prompt: <|sot|> <lang> <|transcribe|>  (timestamps enabled).
        std::vector<int64_t> prompt{kSotToken, languageToken, kTranscribeToken};

        // --- initial decoder run (no past) over the whole prompt ---
        std::unordered_map<std::string, Ort::Value> pastKV; // name -> value
        std::vector<int> generated;
        int minTimestamp = kTimestampBegin;
        bool firstStep = true;
        int nextToken = kEosToken;

        {
            const int64_t idShape[2] = {1, static_cast<int64_t>(prompt.size())};
            Ort::Value idTensor = Ort::Value::CreateTensor<int64_t>(d->mem, prompt.data(),
                                                                    prompt.size(), idShape, 2);
            Ort::Value encView = floatView(encHidden, d->mem);
            Ort::Value ins[2] = {std::move(idTensor), std::move(encView)};
            const char *inN[2] = {d->decInNames[0].c_str(), d->decInNames[1].c_str()};
            std::vector<const char *> outN;
            for (const auto &n : d->decOutNames)
                outN.push_back(n.c_str());
            auto outs = d->decoder->Run(Ort::RunOptions{nullptr}, inN, ins, 2, outN.data(),
                                        outN.size());

            const float *logits = outs[0].GetTensorMutableData<float>();
            const int last = static_cast<int>(prompt.size()) - 1;
            nextToken = d->argmax(logits + static_cast<size_t>(last) * kVocab, minTimestamp,
                                  firstStep);
            firstStep = false;

            for (size_t i = 1; i < d->decOutNames.size(); ++i)
                pastKV.emplace(presentToPast(d->decOutNames[i]), std::move(outs[i]));
        }

        int cachePos = static_cast<int>(prompt.size());

        // --- autoregressive steps with past ---
        for (int step = 0; step < kMaxDecodeTokens; ++step) {
            if (nextToken == kEosToken)
                break;
            if (nextToken >= kTimestampBegin)
                minTimestamp = nextToken;
            generated.push_back(nextToken);

            std::vector<int64_t> idData{nextToken};
            const int64_t idShape[2] = {1, 1};
            Ort::Value idTensor =
                Ort::Value::CreateTensor<int64_t>(d->mem, idData.data(), 1, idShape, 2);
            std::vector<int64_t> cacheData{cachePos};
            const int64_t cacheShape[1] = {1};
            Ort::Value cacheTensor =
                Ort::Value::CreateTensor<int64_t>(d->mem, cacheData.data(), 1, cacheShape, 1);

            std::vector<Ort::Value> ins;
            std::vector<const char *> inN;
            ins.reserve(d->decpInNames.size());
            for (const std::string &name : d->decpInNames) {
                inN.push_back(name.c_str());
                if (name == "input_ids")
                    ins.push_back(std::move(idTensor));
                else if (name == "cache_position")
                    ins.push_back(std::move(cacheTensor));
                else
                    ins.push_back(floatView(pastKV.at(name), d->mem));
            }
            std::vector<const char *> outN;
            for (const auto &n : d->decpOutNames)
                outN.push_back(n.c_str());

            auto outs = d->decoderPast->Run(Ort::RunOptions{nullptr}, inN.data(), ins.data(),
                                            ins.size(), outN.data(), outN.size());

            const float *logits = outs[0].GetTensorMutableData<float>();
            nextToken = d->argmax(logits, minTimestamp, false);

            for (size_t i = 1; i < d->decpOutNames.size(); ++i)
                pastKV.insert_or_assign(presentToPast(d->decpOutNames[i]), std::move(outs[i]));
            ++cachePos;
        }

        // --- segment the generated tokens by timestamp pairs ---
        double lastSegmentEnd = -1.0;
        double segStart = -1.0;
        std::vector<int> textTokens;
        auto flush = [&](double end) {
            if (segStart >= 0.0 && !textTokens.empty()) {
                const QString text = d->tokenizer.decode(textTokens).trimmed();
                const double absStart = windowStartSec + segStart;
                const double absEnd = windowStartSec + end;
                if (!text.isEmpty() && absStart < totalSeconds) {
                    SubtitleCue cue;
                    cue.startUs = secondsToUs(absStart);
                    cue.endUs = secondsToUs(std::min(absEnd, totalSeconds));
                    cue.text = text;
                    if (cue.endUs > cue.startUs)
                        result.cues.append(cue);
                }
            }
            textTokens.clear();
        };

        for (const int tok : generated) {
            if (tok >= kTimestampBegin) {
                const double t = (tok - kTimestampBegin) * 0.02;
                if (segStart < 0.0) {
                    segStart = t;
                } else {
                    flush(t);
                    lastSegmentEnd = t;
                    segStart = t;
                }
            } else {
                textTokens.push_back(tok);
            }
        }

        double advance = (lastSegmentEnd > 0.05) ? lastSegmentEnd : 30.0;
        advance = std::min(advance, 30.0);
        if (advance < 0.05)
            advance = 30.0;
        cursor += static_cast<int>(std::llround(advance * kSampleRate));
    }

    if (progress)
        progress(1.0);
    sortSubtitleCues(result.cues);
    // Pack into short display lines like openai-whisper's VTT writer
    // (word_timestamps + max_line_width=42, max_line_count=1).
    result.cues = packSubtitleCues(result.cues, 42, 1);
    result.ok = true;
    return result;
}

} // namespace drift
