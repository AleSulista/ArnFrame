#pragma once

#include <QDebug>
#include <QString>
#include <QThread>

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <string>
#include <vector>

// Boilerplate every ONNX Runtime user in the engine needs: path conversion, session name
// enumeration, and execution-provider selection. Shared so DRIFT_ORT_EP behaves identically
// everywhere rather than drifting per model.
namespace drift::ort {

inline std::basic_string<ORTCHAR_T> ortPath(const QString &path)
{
#ifdef _WIN32
    return path.toStdWString();
#else
    return path.toStdString();
#endif
}

inline std::vector<std::string> sessionNames(Ort::Session &s, bool inputs)
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

inline std::vector<const char *> cstrs(const std::vector<std::string> &v)
{
    std::vector<const char *> out;
    out.reserve(v.size());
    for (const std::string &s : v)
        out.push_back(s.c_str());
    return out;
}

inline int64_t elementCount(const std::vector<int64_t> &shape)
{
    int64_t n = 1;
    for (const int64_t d : shape)
        n *= d;
    return n;
}

// CUDA is used automatically when the ONNX Runtime build carries the provider and the machine can
// actually load it. DRIFT_ORT_EP overrides the choice: "cpu" forces CPU, "cuda"/"openvino" force
// that provider.
//
// Whether a failure is worth complaining about depends on who asked. An explicit DRIFT_ORT_EP that
// cannot be honoured must be loud, or identical timings look like "the GPU did not help" when in
// fact nothing was accelerated. The automatic attempt failing is ordinary — most machines have no
// CUDA — so that stays at info level.
//
// `sharedArena` registers one env-wide CUDA allocator grown by exactly what is asked for. Only
// pipelines that spread huge allocations across several sessions need it (see Sam2Segmenter);
// for a single small session it is pure overhead.
inline void appendRequestedProvider(Ort::SessionOptions &opts, Ort::Env &env, const char *tag,
                                    bool sharedArena = false)
{
    const QString requested = QString::fromLocal8Bit(qgetenv("DRIFT_ORT_EP")).trimmed().toLower();
    if (requested == QLatin1String("cpu"))
        return;

    QString ep = requested;
    if (ep.isEmpty()) {
        const std::vector<std::string> providers = Ort::GetAvailableProviders();
        if (std::find(providers.begin(), providers.end(), "CUDAExecutionProvider")
            == providers.end())
            return; // CPU-only build: nothing to try, and nothing worth saying about it
        ep = QStringLiteral("cuda");
    }

    try {
        if (ep == QLatin1String("openvino")) {
            opts.AppendExecutionProvider("OpenVINO", {{"device_type", "GPU"}});
        } else if (ep == QLatin1String("cuda")) {
            OrtCUDAProviderOptions cuda;
            cuda.arena_extend_strategy = 1; // kSameAsRequested
            opts.AppendExecutionProvider_CUDA(cuda);

            if (sharedArena) {
                Ort::MemoryInfo cudaMem("Cuda", OrtArenaAllocator, 0, OrtMemTypeDefault);
                Ort::ArenaCfg arena(0 /*max_mem: no cap*/, 1 /*kSameAsRequested*/,
                                    -1 /*initial chunk: default*/, -1 /*max dead bytes: default*/);
                env.CreateAndRegisterAllocatorV2("CUDAExecutionProvider", cudaMem, {}, arena);
                opts.AddConfigEntry("session.use_env_allocators", "1");
            }
        } else {
            qWarning("[%s] DRIFT_ORT_EP=%s is not a provider this build knows; using CPU.", tag,
                     qUtf8Printable(ep));
            return;
        }
        qInfo("[%s] execution provider %s enabled.", tag, qUtf8Printable(ep));
    } catch (const Ort::Exception &e) {
        if (requested.isEmpty()) {
            qInfo("[%s] %s is present but could not be initialised (%s); using CPU.", tag,
                  qUtf8Printable(ep), e.what());
        } else {
            qWarning("[%s] DRIFT_ORT_EP=%s could not be set up (%s). Falling back to CPU; if the "
                     "message mentions loading a shared library, this ONNX Runtime build has no %s "
                     "provider.",
                     tag, qUtf8Printable(ep), e.what(), qUtf8Printable(ep));
        }
    }
}

inline Ort::SessionOptions defaultSessionOptions(Ort::Env &env, const char *tag,
                                                 bool sharedArena = false)
{
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(std::max(1, QThread::idealThreadCount()));
    appendRequestedProvider(opts, env, tag, sharedArena);
    return opts;
}

// Builds a model's sessions, falling back to CPU when an automatically chosen provider turns out
// not to work.
//
// The fallback is not optional. AppendExecutionProvider_CUDA succeeds on any build that ships the
// provider — the device is only touched when a session is actually constructed, which is where
// "no CUDA-capable device is detected" surfaces. So a CUDA-enabled ONNX Runtime on a machine with
// no usable GPU passes every up-front check and then fails every model load. Retrying on CPU is
// what makes that machine merely slower instead of missing the feature entirely.
//
// An explicit DRIFT_ORT_EP is never silently overridden: asking for a provider and quietly getting
// CPU would hide exactly what the variable was set to investigate.
//
// `build` takes a configured Ort::SessionOptions and creates the sessions; it may be called twice,
// so it must overwrite rather than append to whatever it fills.
template <typename BuildFn>
bool buildSessions(Ort::Env &env, const char *tag, bool sharedArena, QString *errorOut,
                   BuildFn &&build)
{
    const bool explicitEp = !QString::fromLocal8Bit(qgetenv("DRIFT_ORT_EP")).trimmed().isEmpty();
    try {
        Ort::SessionOptions opts = defaultSessionOptions(env, tag, sharedArena);
        build(opts);
        return true;
    } catch (const Ort::Exception &e) {
        if (explicitEp) {
            if (errorOut)
                *errorOut = QString::fromUtf8(e.what());
            return false;
        }
        qInfo("[%s] the auto-selected execution provider could not create a session (%s); "
              "retrying on CPU.",
              tag, e.what());
    }

    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(std::max(1, QThread::idealThreadCount()));
        build(opts);
        return true;
    } catch (const Ort::Exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

} // namespace drift::ort
