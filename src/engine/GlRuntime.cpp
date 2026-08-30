#include "GlRuntime.h"

#include "GlModelRenderer.h"

#include <QColor>
#include <QCoreApplication>
#include <QMatrix3x3>
#include <QMutexLocker>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QVector2D>
#include <QVector3D>

#include <cmath>
#include <cstring>
#include <mutex>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif !defined(Q_OS_MACOS)
#include <dlfcn.h>
#endif

// Qt for Android is built against the GLES 2.0 headers so it can still run on ES2-only devices, and
// qopengl.h includes <GLES2/gl2.h> accordingly. The ES 3.0 *functions* this file uses still resolve,
// because QOpenGLExtraFunctions looks them up at runtime — but the ES 3.0 *enum constants* are
// simply absent from the ES2 header, so they are supplied here.
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_RG
#define GL_RG 0x8227
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RG8
#define GL_RG8 0x822B
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif
#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT 0x00000001
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT 0x0002
#endif
#ifndef GL_MAP_INVALIDATE_BUFFER_BIT
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#endif

namespace drift::gl {

namespace {

// Every shader in the project is written as `#version 330 core`. Android has no desktop GL, so the
// version line is swapped for `#version 300 es` and the default precision qualifiers ES requires
// are prepended. Done at the compile sites so package .frag files stay shared with desktop.
QByteArray translateShaderSource(QByteArray body, bool fragment)
{
    if (body.startsWith("#version")) {
        const int newline = body.indexOf('\n');
        body = (newline < 0) ? QByteArray() : body.mid(newline + 1);
    }

    const QOpenGLContext *current = QOpenGLContext::currentContext();
    if (!current || !current->isOpenGLES())
        return QByteArray("#version 330 core\n") + body;

    QByteArray preamble("#version 300 es\n");
    preamble += "precision highp float;\n";
    preamble += "precision highp int;\n";
    if (fragment)
        preamble += "precision highp sampler2D;\n";
    return preamble + body;
}

QByteArray translateShader(const char *source, bool fragment)
{
    return translateShaderSource(QByteArray(source), fragment);
}

QByteArray translateShader(const QString &source, bool fragment)
{
    return translateShaderSource(source.toUtf8(), fragment);
}

constexpr const char *kCopyFragShader = R"(#version 330 core
in vec2 v_texCoord;
out vec4 fragColor;
uniform sampler2D u_currentTexture;
void main() {
    fragColor = texture(u_currentTexture, v_texCoord);
}
)";

// YUV → RGBA with caller-supplied matrix, range and a UV affine for display rotation.
constexpr const char *kNv12FragShader = R"(#version 330 core
in vec2 v_texCoord;
out vec4 fragColor;
uniform sampler2D u_y;
uniform sampler2D u_uv;
uniform mat3 u_yuvToRgb;
uniform vec3 u_yuvOffset;
uniform vec3 u_yuvScale;
uniform mat3 u_texMap;
void main() {
    vec2 src = (u_texMap * vec3(v_texCoord, 1.0)).xy;
    float y = texture(u_y, src).r;
    vec2 chroma = texture(u_uv, src).rg;
    vec3 yuv = (vec3(y, chroma) - u_yuvOffset) * u_yuvScale;
    vec3 rgb = u_yuvToRgb * yuv;
    fragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)";

// Fullscreen triangle strip with standard GL UVs (v=0 at bottom / NDC bottom).
// Source QImages are copied into an FBO once so every pass samples FBO-backed
// textures only (same Y layout). Readback uses toImage(false).
constexpr float kQuad[] = {
    // pos      // uv
    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
    -1.f,  1.f, 0.f, 1.f,
     1.f,  1.f, 1.f, 1.f,
};

QMatrix3x3 yuvToRgbMatrix(int colorspace)
{
    // Row-major, multiplies vec3(Y, Cb, Cr) after range expansion.
    float m[9] = {1.f, 0.f, 1.5748f, 1.f, -0.1873f, -0.4681f, 1.f, 1.8556f, 0.f};
    switch (colorspace) {
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        m[2] = 1.402f;
        m[4] = -0.344f;
        m[5] = -0.714f;
        m[7] = 1.772f;
        break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        m[2] = 1.4746f;
        m[4] = -0.1646f;
        m[5] = -0.5714f;
        m[7] = 1.8814f;
        break;
    default:
        break;
    }
    return QMatrix3x3(m);
}

QMatrix3x3 texMapForRotation(int rotation)
{
    // codedUV = (mat * vec3(displayUV, 1)).xy. 90/270 are clockwise, matching Qt.
    float m[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
    if (rotation == 90) {
        const float r[9] = {0.f, 1.f, 0.f, -1.f, 0.f, 1.f, 0.f, 0.f, 1.f};
        memcpy(m, r, sizeof(m));
    } else if (rotation == 180) {
        const float r[9] = {-1.f, 0.f, 1.f, 0.f, -1.f, 1.f, 0.f, 0.f, 1.f};
        memcpy(m, r, sizeof(m));
    } else if (rotation == 270) {
        const float r[9] = {0.f, -1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        memcpy(m, r, sizeof(m));
    }
    return QMatrix3x3(m);
}

void yuvRangeUniforms(int colorRange, QVector3D *offset, QVector3D *scale)
{
    if (colorRange == AVCOL_RANGE_JPEG) {
        *offset = QVector3D(0.f, 128.f / 255.f, 128.f / 255.f);
        *scale = QVector3D(1.f, 1.f, 1.f);
        return;
    }
    *offset = QVector3D(16.f / 255.f, 128.f / 255.f, 128.f / 255.f);
    *scale = QVector3D(255.f / 219.f, 255.f / 224.f, 255.f / 224.f);
}

bool isHwPixelFormat(AVPixelFormat fmt)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(fmt);
    return desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL);
}

#if !defined(Q_OS_MACOS)
using CUresult = int;
using CUdeviceptr = void *;
using CUarray = void *;
using CUcontext = void *;
using CUstream = void *;
using CUgraphicsResource = void *;

enum { kCuSuccess = 0, kCuMemoryDevice = 2, kCuMemoryArray = 3, kCuRegisterWriteDiscard = 0x02 };

struct CudaMemcpy2D
{
    size_t srcXInBytes = 0;
    size_t srcY = 0;
    int srcMemoryType = 0;
    int srcPad = 0;
    const void *srcHost = nullptr;
    CUdeviceptr srcDevice = nullptr;
    CUarray srcArray = nullptr;
    size_t srcPitch = 0;
    size_t dstXInBytes = 0;
    size_t dstY = 0;
    int dstMemoryType = 0;
    int dstPad = 0;
    void *dstHost = nullptr;
    CUdeviceptr dstDevice = nullptr;
    CUarray dstArray = nullptr;
    size_t dstPitch = 0;
    size_t WidthInBytes = 0;
    size_t Height = 0;
};

struct CudaGlApi
{
    void *lib = nullptr;
    CUresult (*cuInit)(unsigned int) = nullptr;
    CUresult (*cuCtxPushCurrent)(CUcontext) = nullptr;
    CUresult (*cuCtxPopCurrent)(CUcontext *) = nullptr;
    CUresult (*cuGraphicsGLRegisterImage)(CUgraphicsResource *, unsigned int, unsigned int,
                                          unsigned int) = nullptr;
    CUresult (*cuGraphicsUnregisterResource)(CUgraphicsResource) = nullptr;
    CUresult (*cuGraphicsMapResources)(unsigned int, CUgraphicsResource *, CUstream) = nullptr;
    CUresult (*cuGraphicsUnmapResources)(unsigned int, CUgraphicsResource *, CUstream) = nullptr;
    CUresult (*cuGraphicsSubResourceGetMappedArray)(CUarray *, CUgraphicsResource, unsigned int,
                                                    unsigned int) = nullptr;
    CUresult (*cuMemcpy2D)(const CudaMemcpy2D *) = nullptr;
    bool ok = false;
};

CudaGlApi &cudaGlApi()
{
    static CudaGlApi api;
    static std::once_flag once;
    std::call_once(once, [] {
#if defined(Q_OS_WIN)
        api.lib = static_cast<void *>(LoadLibraryW(L"nvcuda.dll"));
        auto sym = [&](const char *name) -> void * {
            return api.lib ? static_cast<void *>(GetProcAddress(static_cast<HMODULE>(api.lib), name))
                           : nullptr;
        };
#else
        api.lib = dlopen("libcuda.so.1", RTLD_LAZY | RTLD_LOCAL);
        auto sym = [&](const char *name) -> void * { return api.lib ? dlsym(api.lib, name) : nullptr; };
#endif
        if (!api.lib)
            return;
#define DRIFT_CUDA_SYM(field, name) \
    api.field = reinterpret_cast<decltype(api.field)>(sym(name)); \
    if (!api.field) \
        return;
        DRIFT_CUDA_SYM(cuInit, "cuInit");
        DRIFT_CUDA_SYM(cuCtxPushCurrent, "cuCtxPushCurrent");
        DRIFT_CUDA_SYM(cuCtxPopCurrent, "cuCtxPopCurrent");
        DRIFT_CUDA_SYM(cuGraphicsGLRegisterImage, "cuGraphicsGLRegisterImage");
        DRIFT_CUDA_SYM(cuGraphicsUnregisterResource, "cuGraphicsUnregisterResource");
        DRIFT_CUDA_SYM(cuGraphicsMapResources, "cuGraphicsMapResources");
        DRIFT_CUDA_SYM(cuGraphicsUnmapResources, "cuGraphicsUnmapResources");
        DRIFT_CUDA_SYM(cuGraphicsSubResourceGetMappedArray, "cuGraphicsSubResourceGetMappedArray");
        DRIFT_CUDA_SYM(cuMemcpy2D, "cuMemcpy2D");
#undef DRIFT_CUDA_SYM
        if (api.cuInit(0) != kCuSuccess)
            return;
        api.ok = true;
    });
    return api;
}

bool cudaContextOf(const AVFrame *frame, CUcontext *ctx, CUstream *stream)
{
    if (!frame || !frame->hw_frames_ctx)
        return false;
    const auto *fc = reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data);
    if (!fc || !fc->device_ctx || fc->device_ctx->type != AV_HWDEVICE_TYPE_CUDA || !fc->device_ctx->hwctx)
        return false;
    const char *hwctx = static_cast<const char *>(fc->device_ctx->hwctx);
    *ctx = *reinterpret_cast<CUcontext const *>(hwctx);
    *stream = *reinterpret_cast<CUstream const *>(hwctx + sizeof(void *));
    return *ctx != nullptr;
}

bool cudaSwFormatIsNv12(const AVFrame *frame)
{
    if (!frame || !frame->hw_frames_ctx)
        return false;
    const auto *fc = reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data);
    return fc && fc->sw_format == AV_PIX_FMT_NV12;
}

bool copyCudaPlaneToTexture(CudaGlApi &api, CUstream stream, CUgraphicsResource resource,
                            CUdeviceptr src, size_t srcPitch, size_t widthBytes, size_t height)
{
    CUarray array = nullptr;
    if (api.cuGraphicsMapResources(1, &resource, stream) != kCuSuccess)
        return false;
    const bool gotArray =
        api.cuGraphicsSubResourceGetMappedArray(&array, resource, 0, 0) == kCuSuccess && array;
    bool copied = false;
    if (gotArray) {
        CudaMemcpy2D op{};
        op.srcMemoryType = kCuMemoryDevice;
        op.srcDevice = src;
        op.srcPitch = srcPitch;
        op.dstMemoryType = kCuMemoryArray;
        op.dstArray = array;
        op.WidthInBytes = widthBytes;
        op.Height = height;
        copied = api.cuMemcpy2D(&op) == kCuSuccess;
    }
    api.cuGraphicsUnmapResources(1, &resource, stream);
    return copied;
}
#endif

uint64_t targetPoolKey(int width, int height, bool wantDepth)
{
    // w/h fit in 31 bits; the low bit tags depth so colour-only and depth targets never share.
    return (uint64_t(uint32_t(width) & 0x7fffffffu) << 33)
           | (uint64_t(uint32_t(height) & 0x7fffffffu) << 1)
           | (wantDepth ? 1u : 0u);
}

GlRuntime *g_runtime = nullptr;

void destroyGpuRuntime()
{
    if (!g_runtime)
        return;
    g_runtime->shutdown();
    delete g_runtime;
    g_runtime = nullptr;
}

} // namespace

const char *const kQuadVertexShader = R"(#version 330 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texCoord;
out vec2 v_texCoord;
void main() {
    v_texCoord = a_texCoord;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

GlRuntime &runtime()
{
    // Reached from the compositor thread, the export job and the GUI thread, so
    // the lazy construction has to be race-free — two runtimes would mean two GL
    // contexts and two post-routines.
    static std::once_flag once;
    std::call_once(once, [] {
        g_runtime = new GlRuntime;
        qAddPostRoutine(destroyGpuRuntime);
    });
    return *g_runtime;
}

bool GlRuntime::initGlObjects()
{
    QOpenGLContext *shared = QOpenGLContext::globalShareContext();
    const bool sharingRequired = QCoreApplication::testAttribute(Qt::AA_ShareOpenGLContexts);

    // In the application, use the share context's realized format: EGL may
    // adjust the requested format during creation. Command-line tools and
    // tests have no Qt Quick share context, so retain an independent context.
    if (shared && !shared->isValid())
        shared = nullptr;

    if (sharingRequired && !shared) {
        qWarning("GlRuntime: required global OpenGL share context is unavailable");
        return false;
    }

    QSurfaceFormat format;
    if (shared) {
        format = shared->format();
    } else if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
        // Asking for a 3.3 core profile on Android makes QOpenGLContext::create() fail outright.
        format.setRenderableType(QSurfaceFormat::OpenGLES);
        format.setVersion(3, 0);
        format.setProfile(QSurfaceFormat::NoProfile);
        format.setDepthBufferSize(0);
        format.setStencilBufferSize(0);
    } else {
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(3, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setDepthBufferSize(0);
        format.setStencilBufferSize(0);
    }

    surface = std::make_unique<QOffscreenSurface>();
    surface->setFormat(format);
    surface->create();
    if (!surface->isValid()) {
        qWarning("GlRuntime: failed to create offscreen surface");
        surface.reset();
        return false;
    }

    context = std::make_unique<QOpenGLContext>();
    if (shared)
        context->setShareContext(shared);
    context->setFormat(format);
    if (!context->create()) {
        qWarning("GlRuntime: failed to create OpenGL context");
        context.reset();
        surface.reset();
        return false;
    }

    // create() succeeds even when the driver refuses to share, dropping the request
    // on the floor, so whether sharing actually happened has to be read back.
    m_sharesWithGui = shared && QOpenGLContext::areSharing(context.get(), shared);

    if (!context->makeCurrent(surface.get())) {
        qWarning("GlRuntime: makeCurrent failed on the GL thread");
        context.reset();
        surface.reset();
        return false;
    }

    auto *gl = context->extraFunctions();
    if (!gl) {
        qWarning("GlRuntime: OpenGL extra functions unavailable");
        context->doneCurrent();
        return false;
    }

    // setVersion() above is a request; the driver decides. Everything after this line
    // assumes ES 3.0 / GL 3.3.
    const bool isEs = context->isOpenGLES();
    const int major = context->format().majorVersion();
    const int minor = context->format().minorVersion();
    if (isEs ? major < 3 : (major < 3 || (major == 3 && minor < 3))) {
        qCritical("GlRuntime: this device reports OpenGL%s %d.%d; Drift needs OpenGL ES 3.0 or "
                  "OpenGL 3.3. GPU rendering is unavailable. (vendor: %s, renderer: %s)",
                  isEs ? " ES" : "", major, minor,
                  reinterpret_cast<const char *>(gl->glGetString(GL_VENDOR)),
                  reinterpret_cast<const char *>(gl->glGetString(GL_RENDERER)));
        context->doneCurrent();
        context.reset();
        surface.reset();
        return false;
    }

    gl->glGenVertexArrays(1, &vao);
    gl->glBindVertexArray(vao);
    gl->glGenBuffers(1, &vbo);
    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(0));
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void *>(2 * sizeof(float)));
    gl->glBindVertexArray(0);

    copyProgram = std::make_unique<QOpenGLShaderProgram>();
    if (!copyProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                              translateShader(kQuadVertexShader, false))
        || !copyProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                 translateShader(kCopyFragShader, true))
        || !copyProgram->link()) {
        qWarning("GlRuntime: copy shader failed: %s", qPrintable(copyProgram->log()));
        copyProgram.reset();
        context->doneCurrent();
        return false;
    }

    context->doneCurrent();
    return true;
}

bool GlRuntime::ensureReady()
{
    QMutexLocker lock(&m_initMutex);
    if (m_initTried)
        return m_ok;
    m_initTried = true;

    if (!QCoreApplication::instance()) {
        qWarning("GlRuntime: no QCoreApplication; OpenGL unavailable");
        return false;
    }

    m_glThread = new QThread;
    m_glThread->setObjectName(QStringLiteral("DriftGL"));
    m_glThread->start();

    m_glOwner = new QObject;
    m_glOwner->moveToThread(m_glThread);

    bool initialized = false;
    QMetaObject::invokeMethod(
        m_glOwner, [this] { return initGlObjects(); }, Qt::BlockingQueuedConnection, &initialized);

    if (!initialized) {
        delete m_glOwner;
        m_glOwner = nullptr;
        m_glThread->quit();
        m_glThread->wait();
        delete m_glThread;
        m_glThread = nullptr;
        return false;
    }

    m_ok = true;
    return m_ok;
}

bool GlRuntime::available()
{
    return ensureReady();
}

bool GlRuntime::sharesWithGuiContext()
{
    return ensureReady() && m_sharesWithGui;
}

bool GlRuntime::exec(const std::function<void()> &fn)
{
    if (!ensureReady())
        return false;

    bool ran = false;
    QMetaObject::invokeMethod(
        m_glOwner,
        [this, &fn]() -> bool {
            if (!context->makeCurrent(surface.get())) {
                qWarning("GlRuntime: makeCurrent failed");
                return false;
            }
            fn();
            context->doneCurrent();
            return true;
        },
        Qt::BlockingQueuedConnection, &ran);
    return ran;
}

QOpenGLExtraFunctions *GlRuntime::functions()
{
    return context ? context->extraFunctions() : nullptr;
}

void GlRuntime::releaseCaches()
{
    {
        QMutexLocker lock(&m_initMutex);
        if (!m_ok)
            return;
    }

    exec([this] {
        destroyImageUploadCache();
        destroyVideoUploadState();
        m_targetPool.clear();
        m_pooledTargets = 0;
        if (auto *gl = functions())
            destroyGlModels(*this, gl);
    });
}

void GlRuntime::shutdown()
{
    {
        QMutexLocker lock(&m_initMutex);
        if (!m_ok || !m_glOwner)
            return;
    }

    QMetaObject::invokeMethod(
        m_glOwner,
        [this] {
            if (!context->makeCurrent(surface.get()))
                return;
            if (auto *gl = context->extraFunctions()) {
                for (int i = 0; i < kPresentRingSize; ++i) {
                    if (m_presentFence[i]) {
                        gl->glDeleteSync(m_presentFence[i]);
                        m_presentFence[i] = nullptr;
                    }
                }
                destroyImageUploadCache();
                destroyVideoUploadState();
            }
            for (GlTarget &target : m_presentRing)
                target.fbo.reset();
            m_targetPool.clear();
            m_pooledTargets = 0;
            programs.clear();
            copyProgram.reset();
            if (auto *gl = context->extraFunctions()) {
                destroyGlModels(*this, gl);
                for (const auto &entry : staticTextures) {
                    GLuint tex = entry.second;
                    gl->glDeleteTextures(1, &tex);
                }
                staticTextures.clear();
                for (const auto &entry : faceSwapPhotos) {
                    if (entry.second.texture)
                        gl->glDeleteTextures(1, &entry.second.texture);
                    if (entry.second.lowFreq)
                        gl->glDeleteTextures(1, &entry.second.lowFreq);
                }
                faceSwapPhotos.clear();
                if (vbo) {
                    gl->glDeleteBuffers(1, &vbo);
                    vbo = 0;
                }
                if (vao) {
                    gl->glDeleteVertexArrays(1, &vao);
                    vao = 0;
                }
            }
            context->doneCurrent();
        },
        Qt::BlockingQueuedConnection);

    // Stop the thread before deleting the object that lives on it — deleting a
    // QObject from a thread other than its own is undefined behaviour.
    m_glThread->quit();
    m_glThread->wait();
    delete m_glOwner;
    m_glOwner = nullptr;
    delete m_glThread;
    m_glThread = nullptr;

    context.reset();
    surface.reset();
    m_ok = false;
}


GlTarget GlRuntime::acquireTarget(int width, int height, bool wantDepth)
{
    GlTarget target;
    target.width = qMax(1, width);
    target.height = qMax(1, height);
    target.hasDepth = wantDepth;

    const uint64_t key = targetPoolKey(target.width, target.height, wantDepth);
    const auto it = m_targetPool.find(key);
    if (it != m_targetPool.end()) {
        target.fbo = std::move(it->second);
        m_targetPool.erase(it);
        --m_pooledTargets;
        return target;
    }

    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(wantDepth ? QOpenGLFramebufferObject::Depth
                                : QOpenGLFramebufferObject::NoAttachment);
    target.fbo = std::make_unique<QOpenGLFramebufferObject>(target.width, target.height, fmt);
    return target;
}

void GlRuntime::releaseTarget(GlTarget &&target)
{
    if (!target.isValid())
        return;
    if (m_pooledTargets >= kMaxPooledTargets)
        return; // let it drop

    const uint64_t key = targetPoolKey(target.width, target.height, target.hasDepth);
    m_targetPool.emplace(key, std::move(target.fbo));
    ++m_pooledTargets;
}

QImage GlRuntime::readTarget(const GlTarget &target)
{
    if (!target.isValid())
        return {};
    if (auto *gl = functions())
        gl->glFinish();
    return target.fbo->toImage(false).convertToFormat(QImage::Format_RGBA8888);
}

void GlRuntime::waitPresentFence(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= kPresentRingSize)
        return;
    GLsync &fence = m_presentFence[slotIndex];
    if (!fence)
        return;
    auto *gl = functions();
    if (!gl) {
        fence = nullptr;
        return;
    }
    // Only wait for this slot's prior publish — not the whole GPU pipeline.
    gl->glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, GLuint64(100'000'000)); // 100 ms
    gl->glDeleteSync(fence);
    fence = nullptr;
}

void GlRuntime::destroyImageUploadCache()
{
    auto *gl = functions();
    for (CachedUpload &entry : m_imageUploadLru) {
        if (gl && entry.texture)
            gl->glDeleteTextures(1, &entry.texture);
        entry.texture = 0;
    }
    m_imageUploadLru.clear();
    m_imageUploadIndex.clear();
}

void GlRuntime::destroyVideoUploadState()
{
    unregisterCudaResources();
    auto *gl = functions();
    if (gl) {
        if (m_videoY) {
            gl->glDeleteTextures(1, &m_videoY);
            m_videoY = 0;
        }
        if (m_videoUV) {
            gl->glDeleteTextures(1, &m_videoUV);
            m_videoUV = 0;
        }
        if (m_videoPbo[0] || m_videoPbo[1]) {
            gl->glDeleteBuffers(2, m_videoPbo);
            m_videoPbo[0] = m_videoPbo[1] = 0;
        }
    }
    m_videoTexW = 0;
    m_videoTexH = 0;
    m_videoPboIndex = 0;
    av_frame_free(&m_hwImportStaging);
    sws_freeContext(m_importSws);
    m_importSws = nullptr;
}

bool GlRuntime::ensureVideoUploadTextures(QOpenGLExtraFunctions *gl, int width, int height)
{
    if (!gl || width < 2 || height < 2 || (width % 2) || (height % 2))
        return false;
    if (m_videoY && m_videoUV && m_videoTexW == width && m_videoTexH == height)
        return true;

    unregisterCudaResources();
    if (m_videoY)
        gl->glDeleteTextures(1, &m_videoY);
    if (m_videoUV)
        gl->glDeleteTextures(1, &m_videoUV);
    m_videoY = m_videoUV = 0;

    gl->glGenTextures(1, &m_videoY);
    gl->glBindTexture(GL_TEXTURE_2D, m_videoY);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    gl->glGenTextures(1, &m_videoUV);
    gl->glBindTexture(GL_TEXTURE_2D, m_videoUV);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width / 2, height / 2, 0, GL_RG, GL_UNSIGNED_BYTE,
                     nullptr);

    m_videoTexW = width;
    m_videoTexH = height;
    return m_videoY != 0 && m_videoUV != 0;
}

bool GlRuntime::uploadPlanePbo(QOpenGLExtraFunctions *gl, GLuint texture, int texW, int texH,
                               GLenum internalFormat, GLenum format, const uint8_t *src, int srcPitch,
                               int packedWidth)
{
    Q_UNUSED(internalFormat);
    if (!gl || !texture || !src || texW <= 0 || texH <= 0 || packedWidth <= 0 || srcPitch <= 0)
        return false;
    const qsizetype packed = qsizetype(packedWidth) * texH;
    if (!m_videoPbo[0])
        gl->glGenBuffers(2, m_videoPbo);
    const GLuint pbo = m_videoPbo[m_videoPboIndex];
    m_videoPboIndex ^= 1;
    gl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    gl->glBufferData(GL_PIXEL_UNPACK_BUFFER, packed, nullptr, GL_STREAM_DRAW);
    void *dst = gl->glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, packed,
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (!dst) {
        gl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        return false;
    }
    if (srcPitch == packedWidth) {
        memcpy(dst, src, size_t(packed));
    } else {
        auto *out = static_cast<uint8_t *>(dst);
        const int rowBytes = qMin(packedWidth, srcPitch);
        for (int y = 0; y < texH; ++y)
            memcpy(out + size_t(y) * packedWidth, src + size_t(y) * srcPitch, size_t(rowBytes));
    }
    gl->glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    gl->glBindTexture(GL_TEXTURE_2D, texture);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW, texH, format, GL_UNSIGNED_BYTE, nullptr);
    gl->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return true;
}

void GlRuntime::unregisterCudaResources()
{
#if !defined(Q_OS_MACOS)
    CudaGlApi &api = cudaGlApi();
    if (!api.ok)
        return;
    if (m_cudaYResource) {
        api.cuGraphicsUnregisterResource(static_cast<CUgraphicsResource>(m_cudaYResource));
        m_cudaYResource = nullptr;
    }
    if (m_cudaUvResource) {
        api.cuGraphicsUnregisterResource(static_cast<CUgraphicsResource>(m_cudaUvResource));
        m_cudaUvResource = nullptr;
    }
    m_cudaTexW = 0;
    m_cudaTexH = 0;
#endif
}

bool GlRuntime::importCudaNv12(QOpenGLExtraFunctions *gl, const AVFrame *frame)
{
#if defined(Q_OS_MACOS)
    Q_UNUSED(gl);
    Q_UNUSED(frame);
    return false;
#else
    if (m_cudaImportFailed || !frame || frame->format != AV_PIX_FMT_CUDA || !cudaSwFormatIsNv12(frame))
        return false;
    CudaGlApi &api = cudaGlApi();
    if (!api.ok) {
        m_cudaImportFailed = true;
        return false;
    }
    CUcontext ctx = nullptr;
    CUstream stream = nullptr;
    if (!cudaContextOf(frame, &ctx, &stream))
        return false;

    const int w = frame->width;
    const int h = frame->height;
    if (!ensureVideoUploadTextures(gl, w, h))
        return false;

    if (api.cuCtxPushCurrent(ctx) != kCuSuccess)
        return false;

    bool ok = false;
    if (m_cudaTexW != w || m_cudaTexH != h || !m_cudaYResource || !m_cudaUvResource) {
        unregisterCudaResources();
        CUgraphicsResource yRes = nullptr;
        CUgraphicsResource uvRes = nullptr;
        if (api.cuGraphicsGLRegisterImage(&yRes, m_videoY, GL_TEXTURE_2D, kCuRegisterWriteDiscard)
                == kCuSuccess
            && api.cuGraphicsGLRegisterImage(&uvRes, m_videoUV, GL_TEXTURE_2D, kCuRegisterWriteDiscard)
                == kCuSuccess) {
            m_cudaYResource = yRes;
            m_cudaUvResource = uvRes;
            m_cudaTexW = w;
            m_cudaTexH = h;
        } else {
            if (yRes)
                api.cuGraphicsUnregisterResource(yRes);
            if (uvRes)
                api.cuGraphicsUnregisterResource(uvRes);
            m_cudaImportFailed = true;
        }
    }

    if (m_cudaYResource && m_cudaUvResource) {
        ok = copyCudaPlaneToTexture(api, stream, static_cast<CUgraphicsResource>(m_cudaYResource),
                                    frame->data[0], size_t(qMax(0, frame->linesize[0])), size_t(w),
                                    size_t(h))
            && copyCudaPlaneToTexture(api, stream, static_cast<CUgraphicsResource>(m_cudaUvResource),
                                      frame->data[1], size_t(qMax(0, frame->linesize[1])), size_t(w),
                                      size_t(h / 2));
        if (!ok)
            m_cudaImportFailed = true;
    }

    CUcontext popped = nullptr;
    api.cuCtxPopCurrent(&popped);
    return ok;
#endif
}

AVFrame *GlRuntime::ensureSoftwareNv12(const AVFrame *src)
{
    if (!src)
        return nullptr;
    if (src->format == AV_PIX_FMT_NV12)
        return const_cast<AVFrame *>(src);

    if (isHwPixelFormat(static_cast<AVPixelFormat>(src->format))) {
        if (!m_hwImportStaging)
            m_hwImportStaging = av_frame_alloc();
        if (!m_hwImportStaging)
            return nullptr;
        av_frame_unref(m_hwImportStaging);
        if (av_hwframe_transfer_data(m_hwImportStaging, src, 0) < 0) {
            av_frame_unref(m_hwImportStaging);
            return nullptr;
        }
        src = m_hwImportStaging;
        if (src->format == AV_PIX_FMT_NV12)
            return m_hwImportStaging;
    }

    const int tw = src->width & ~1;
    const int th = src->height & ~1;
    if (tw < 2 || th < 2)
        return nullptr;

    m_importSws = sws_getCachedContext(m_importSws, src->width, src->height,
                                       static_cast<AVPixelFormat>(src->format), tw, th, AV_PIX_FMT_NV12,
                                       SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_importSws)
        return nullptr;

    if (!m_hwImportStaging)
        m_hwImportStaging = av_frame_alloc();
    if (!m_hwImportStaging)
        return nullptr;
    if (m_hwImportStaging->format != AV_PIX_FMT_NV12 || m_hwImportStaging->width != tw
        || m_hwImportStaging->height != th || !m_hwImportStaging->data[0]) {
        av_frame_unref(m_hwImportStaging);
        m_hwImportStaging->format = AV_PIX_FMT_NV12;
        m_hwImportStaging->width = tw;
        m_hwImportStaging->height = th;
        if (av_frame_get_buffer(m_hwImportStaging, 0) < 0) {
            av_frame_unref(m_hwImportStaging);
            return nullptr;
        }
    }
    sws_scale(m_importSws, src->data, src->linesize, 0, src->height, m_hwImportStaging->data,
              m_hwImportStaging->linesize);
    m_hwImportStaging->colorspace = src->colorspace;
    m_hwImportStaging->color_range = src->color_range;
    return m_hwImportStaging;
}

GlTarget &GlRuntime::acquirePresentTarget(int width, int height)
{
    const int w = qMax(1, width);
    const int h = qMax(1, height);

    const int slotIndex = m_presentNext;
    GlTarget &slot = m_presentRing[slotIndex];
    m_presentNext = (m_presentNext + 1) % kPresentRingSize;

    // The scene graph may still be sampling this ring slot from a previous publish.
    // Wait for that fence before redrawing into the same FBO.
    waitPresentFence(slotIndex);

    if (!slot.isValid() || slot.width != w || slot.height != h) {
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::NoAttachment);
        slot.fbo = std::make_unique<QOpenGLFramebufferObject>(w, h, fmt);
        slot.width = w;
        slot.height = h;
    }
    return slot;
}

void GlRuntime::markPresentReady(GlTarget &presentTarget)
{
    auto *gl = functions();
    if (!gl || !presentTarget.isValid())
        return;

    int slotIndex = -1;
    for (int i = 0; i < kPresentRingSize; ++i) {
        if (&m_presentRing[i] == &presentTarget) {
            slotIndex = i;
            break;
        }
    }
    if (slotIndex < 0)
        return;

    // Publish without a client wait: Qt Quick draws on the next vsync. The ring
    // waits this fence in acquirePresentTarget before reuse of the same slot.
    if (m_presentFence[slotIndex]) {
        gl->glDeleteSync(m_presentFence[slotIndex]);
        m_presentFence[slotIndex] = nullptr;
    }
    gl->glFlush();
    m_presentFence[slotIndex] = gl->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

QOpenGLShaderProgram *GlRuntime::builtinProgram(const QString &id, const char *vertexSource,
                                                const char *fragmentSource)
{
    return builtinProgram(id, vertexSource, fragmentSource, nullptr);
}

QOpenGLShaderProgram *GlRuntime::builtinProgram(const QString &id, const char *vertexSource,
                                                const char *fragmentSource, const char *geom)
{
    CompiledEffect &cached = programs[id];
    if (cached.ok)
        return cached.passes[0].program.get();

    cached = CompiledEffect{};
    cached.id = id;

    CompiledPass pass;
    pass.program = std::make_unique<QOpenGLShaderProgram>();
    if (!pass.program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                               translateShader(vertexSource, false))
        || (geom && !pass.program->addShaderFromSourceCode(QOpenGLShader::Geometry, geom))
        || !pass.program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                  translateShader(fragmentSource, true))
        || !pass.program->link()) {
        qWarning("GlRuntime: builtin program '%s' failed: %s", qPrintable(id),
                 qPrintable(pass.program->log()));
        programs.erase(id);
        return nullptr;
    }

    cached.passes.push_back(std::move(pass));
    cached.ok = true;
    return cached.passes[0].program.get();
}

CompiledEffect *GlRuntime::compile(const QString &cacheKey, const drift::GpuEffectDefinition &gpu)
{
    QString sourceSig;
    for (const drift::GpuEffectPass &pass : gpu.passes)
        sourceSig += pass.fragmentShaderSource;
    sourceSig += QLatin1Char('#');
    sourceSig += QString::number(gpu.passes.size());

    CompiledEffect &cached = programs[cacheKey];
    if (cached.ok && cached.id == cacheKey && cached.sourceSig == sourceSig)
        return &cached;

    cached = CompiledEffect{};
    cached.id = cacheKey;
    cached.sourceSig = sourceSig;
    cached.passes.reserve(static_cast<size_t>(gpu.passes.size()));

    for (const drift::GpuEffectPass &pass : gpu.passes) {
        CompiledPass cp;
        cp.program = std::make_unique<QOpenGLShaderProgram>();
        if (!cp.program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                translateShader(kQuadVertexShader, false))) {
            qWarning("GlRuntime: vertex shader compile failed for %s: %s", qPrintable(cacheKey),
                     qPrintable(cp.program->log()));
            programs.erase(cacheKey);
            return nullptr;
        }
        if (!cp.program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                translateShader(pass.fragmentShaderSource, true))) {
            qWarning("GlRuntime: fragment compile failed for %s pass %d (%s): %s", qPrintable(cacheKey),
                     pass.passIndex, qPrintable(pass.fragmentShaderFile), qPrintable(cp.program->log()));
            programs.erase(cacheKey);
            return nullptr;
        }
        if (!cp.program->link()) {
            qWarning("GlRuntime: link failed for %s pass %d: %s", qPrintable(cacheKey), pass.passIndex,
                     qPrintable(cp.program->log()));
            programs.erase(cacheKey);
            return nullptr;
        }
        cached.passes.push_back(std::move(cp));
    }
    cached.ok = true;
    return &cached;
}

GLuint uploadTexture(QOpenGLExtraFunctions *gl, const QImage &image, bool flipVertically)
{
    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    if (flipVertically)
        rgba = rgba.mirrored(false, true);
    GLuint tex = 0;
    gl->glGenTextures(1, &tex);
    gl->glBindTexture(GL_TEXTURE_2D, tex);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.width(), rgba.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     rgba.constBits());
    return tex;
}

bool blitTextureToTarget(GlRuntime &rt, QOpenGLExtraFunctions *gl, GLuint srcTex, GlTarget &dest)
{
    if (!rt.copyProgram || !dest.isValid())
        return false;
    dest.fbo->bind();
    gl->glViewport(0, 0, dest.width, dest.height);
    gl->glDisable(GL_BLEND);
    gl->glClearColor(0.f, 0.f, 0.f, 0.f);
    gl->glClear(GL_COLOR_BUFFER_BIT);
    rt.copyProgram->bind();
    rt.copyProgram->setUniformValue("u_currentTexture", 0);
    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, srcTex);
    gl->glBindVertexArray(rt.vao);
    gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl->glBindVertexArray(0);
    rt.copyProgram->release();
    dest.fbo->release();
    return true;
}

// Static package assets live for the process lifetime. They are plain uploads (not FBO-backed),
// so they must be flipped to match the Y layout of the FBO-promoted source targets.
GLuint staticTexture(GlRuntime &rt, QOpenGLExtraFunctions *gl, const QString &path)
{
    const auto it = rt.staticTextures.find(path);
    if (it != rt.staticTextures.end())
        return it->second;

    QImage image;
    if (!image.load(path) || image.isNull()) {
        qWarning("GlRuntime: failed to load texture '%s'", qPrintable(path));
        rt.staticTextures[path] = 0;
        return 0;
    }
    const GLuint tex = uploadTexture(gl, image, /*flipVertically=*/true);
    gl->glBindTexture(GL_TEXTURE_2D, tex);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    rt.staticTextures[path] = tex;
    return tex;
}

GLuint cachedUploadTexture(GlRuntime &rt, QOpenGLExtraFunctions *gl, const QImage &image)
{
    if (image.isNull() || !gl)
        return 0;

    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    const qint64 key = rgba.cacheKey();
    auto indexIt = rt.m_imageUploadIndex.find(key);
    if (indexIt != rt.m_imageUploadIndex.end()) {
        // LRU touch.
        rt.m_imageUploadLru.splice(rt.m_imageUploadLru.begin(), rt.m_imageUploadLru, indexIt->second);
        return indexIt->second->texture;
    }

    const GLuint tex = uploadTexture(gl, rgba);
    if (!tex)
        return 0;

    while (rt.m_imageUploadLru.size() >= GlRuntime::kMaxCachedUploads) {
        GlRuntime::CachedUpload &old = rt.m_imageUploadLru.back();
        rt.m_imageUploadIndex.erase(old.cacheKey);
        if (old.texture)
            gl->glDeleteTextures(1, &old.texture);
        rt.m_imageUploadLru.pop_back();
    }

    rt.m_imageUploadLru.push_front(GlRuntime::CachedUpload{key, tex, rgba.width(), rgba.height()});
    rt.m_imageUploadIndex[key] = rt.m_imageUploadLru.begin();
    return tex;
}

GlTarget promoteImageToTarget(GlRuntime &rt, QOpenGLExtraFunctions *gl, const QImage &image,
                              const QSize &fallbackSize)
{
    QImage rgba = image.isNull() ? QImage(fallbackSize, QImage::Format_RGBA8888)
                                 : image.convertToFormat(QImage::Format_RGBA8888);
    if (image.isNull())
        rgba.fill(Qt::transparent);

    GlTarget target = rt.acquireTarget(rgba.width(), rgba.height());
    if (!target.isValid())
        return {};

    const GLuint uploaded = uploadTexture(gl, rgba);
    const bool ok = blitTextureToTarget(rt, gl, uploaded, target);
    gl->glDeleteTextures(1, &uploaded);
    if (!ok) {
        rt.releaseTarget(std::move(target));
        return {};
    }
    return target;
}

GlTarget promoteImageToTargetCached(GlRuntime &rt, QOpenGLExtraFunctions *gl, const QImage &image,
                                    const QSize &fallbackSize)
{
    if (image.isNull())
        return promoteImageToTarget(rt, gl, image, fallbackSize);

    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    GlTarget target = rt.acquireTarget(rgba.width(), rgba.height());
    if (!target.isValid())
        return {};

    const GLuint uploaded = cachedUploadTexture(rt, gl, rgba);
    if (!uploaded) {
        rt.releaseTarget(std::move(target));
        return {};
    }
    const bool ok = blitTextureToTarget(rt, gl, uploaded, target);
    // uploaded stays in the LRU cache — do not delete.
    if (!ok) {
        rt.releaseTarget(std::move(target));
        return {};
    }
    return target;
}

GlTarget promoteVideoFrameToTarget(GlRuntime &rt, QOpenGLExtraFunctions *gl,
                                   const PreviewVideoFrame &frame)
{
    if (!gl || !frame.isValid())
        return {};

    const AVFrame *av = frame.frame.get();
    const int codedW = av->width & ~1;
    const int codedH = av->height & ~1;
    if (codedW < 2 || codedH < 2)
        return {};

    bool uploaded = rt.importCudaNv12(gl, av);
    if (!uploaded) {
        AVFrame *nv12 = rt.ensureSoftwareNv12(av);
        if (!nv12 || nv12->format != AV_PIX_FMT_NV12)
            return {};
        const int w = nv12->width;
        const int h = nv12->height;
        if (!rt.ensureVideoUploadTextures(gl, w, h))
            return {};
        if (!rt.uploadPlanePbo(gl, rt.m_videoY, w, h, GL_R8, GL_RED, nv12->data[0], nv12->linesize[0], w)
            || !rt.uploadPlanePbo(gl, rt.m_videoUV, w / 2, h / 2, GL_RG8, GL_RG, nv12->data[1],
                                  nv12->linesize[1], w))
            return {};
    }

    const int destW = qMax(2, frame.displayWidth() & ~1);
    const int destH = qMax(2, frame.displayHeight() & ~1);
    GlTarget target = rt.acquireTarget(destW, destH);
    if (!target.isValid())
        return {};

    QOpenGLShaderProgram *program =
        rt.builtinProgram(QStringLiteral("__nv12__"), kQuadVertexShader, kNv12FragShader);
    if (!program) {
        rt.releaseTarget(std::move(target));
        return {};
    }

    QVector3D offset;
    QVector3D scale;
    yuvRangeUniforms(frame.colorRange, &offset, &scale);

    target.fbo->bind();
    gl->glViewport(0, 0, destW, destH);
    gl->glDisable(GL_BLEND);
    gl->glClearColor(0.f, 0.f, 0.f, 0.f);
    gl->glClear(GL_COLOR_BUFFER_BIT);
    program->bind();
    program->setUniformValue("u_y", 0);
    program->setUniformValue("u_uv", 1);
    program->setUniformValue("u_yuvToRgb", yuvToRgbMatrix(frame.colorspace));
    program->setUniformValue("u_yuvOffset", offset);
    program->setUniformValue("u_yuvScale", scale);
    program->setUniformValue("u_texMap", texMapForRotation(frame.rotation));
    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, rt.m_videoY);
    gl->glActiveTexture(GL_TEXTURE1);
    gl->glBindTexture(GL_TEXTURE_2D, rt.m_videoUV);
    gl->glBindVertexArray(rt.vao);
    gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl->glBindVertexArray(0);
    program->release();
    target.fbo->release();
    return target;
}

void setPackageUniforms(QOpenGLShaderProgram *program, const QMap<QString, QVariant> &parameters,
                        const QSize &resolution, drift::TimeUs timeUs, double progress)
{
    program->setUniformValue("u_currentTexture", 0);
    program->setUniformValue("u_resolution",
                             QVector2D(float(resolution.width()), float(resolution.height())));
    const float timeSec = float(timeUs) / 1'000'000.f;
    program->setUniformValue("u_time", timeSec);
    program->setUniformValue("u_frameIndex", int(std::floor(timeSec * 30.f)));
    // Integer microseconds for hash-stable glitch effects that mirror CPU seeding.
    program->setUniformValue("u_timeUs", float(timeUs));
    program->setUniformValue("u_progress", float(qBound(0.0, progress, 1.0)));

    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        if (drift::isEngineBoundGpuUniform(it.key()))
            continue;
        const int loc = program->uniformLocation(it.key());
        if (loc < 0)
            continue;
        const QVariant &value = it.value();
        if (value.typeId() == QMetaType::Bool) {
            program->setUniformValue(loc, value.toBool() ? 1.f : 0.f);
        } else if (value.userType() == qMetaTypeId<drift::GpuFloatArray>()) {
            const auto array = value.value<drift::GpuFloatArray>();
            if (array.tupleSize > 0 && !array.values.isEmpty()) {
                program->setUniformValueArray(loc, array.values.constData(),
                                              array.values.size() / array.tupleSize,
                                              array.tupleSize);
            }
        } else if (value.typeId() == QMetaType::QString) {
            const QString s = value.toString();
            // File-path params must not fall through to s.toFloat(). A path currently binds as
            // 0.0, which is harmless today but bites the moment a gpu package grows a file param.
            if (s.contains(QLatin1Char('/')) || s.endsWith(QLatin1String(".glb"))
                || s.endsWith(QLatin1String(".gltf"))) {
                continue;
            }
            if (s.startsWith(QLatin1Char('#'))) {
                const QColor c(s);
                program->setUniformValue(loc,
                                         QVector3D(float(c.redF()), float(c.greenF()), float(c.blueF())));
            } else if (it.key() == QLatin1String("position") || it.key() == QLatin1String("blendMode")) {
                float mode = 0.f;
                if (s == QLatin1String("right") || s == QLatin1String("add"))
                    mode = 1.f;
                else if (s == QLatin1String("top") || s == QLatin1String("screen"))
                    mode = 2.f;
                else if (s == QLatin1String("bottom"))
                    mode = 3.f;
                else if (s == QLatin1String("random"))
                    mode = 4.f;
                program->setUniformValue(loc, mode);
            } else {
                program->setUniformValue(loc, s.toFloat());
            }
        } else {
            program->setUniformValue(loc, float(value.toDouble()));
        }
    }
}

GlTarget runPipeline(GlRuntime &rt, QOpenGLExtraFunctions *gl, const QString &cacheKey,
                     const drift::GpuEffectDefinition &gpu, const std::vector<const GlTarget *> &sources,
                     const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs, double progress,
                     const QSize &canvasSize)
{
    CompiledEffect *compiled = rt.compile(cacheKey, gpu);
    if (!compiled || !compiled->ok || compiled->passes.size() != size_t(gpu.passes.size()))
        return {};

    auto sourceTexAt = [&](int index) -> GLuint {
        if (index < 0 || index >= int(sources.size()))
            return sources.empty() ? 0 : sources[0]->texture();
        return sources[size_t(index)]->texture();
    };

    std::map<QString, GlTarget> buffers;
    bool failed = false;
    for (const drift::GpuEffectBufferSpec &spec : gpu.intermediateBuffers) {
        const int w = qMax(1, int(std::lround(canvasSize.width() * spec.scale)));
        const int h = qMax(1, int(std::lround(canvasSize.height() * spec.scale)));
        GlTarget target = rt.acquireTarget(w, h);
        if (!target.isValid()) {
            qWarning("GlRuntime: FBO alloc failed for buffer %s", qPrintable(spec.id));
            failed = true;
            break;
        }
        buffers.emplace(spec.id, std::move(target));
    }

    std::map<QString, GLuint> textures;
    for (const drift::GpuEffectTextureSpec &spec : gpu.textures)
        textures[spec.id] = staticTexture(rt, gl, spec.path);

    GlTarget canvas = rt.acquireTarget(canvasSize.width(), canvasSize.height());
    if (!canvas.isValid()) {
        qWarning("GlRuntime: canvas FBO alloc failed");
        failed = true;
    }

    for (int i = 0; !failed && i < gpu.passes.size(); ++i) {
        const drift::GpuEffectPass &pass = gpu.passes[i];
        QOpenGLShaderProgram *program = compiled->passes[size_t(i)].program.get();

        QSize inputSize = canvasSize;

        GlTarget *outTarget = nullptr;
        if (pass.output.type == drift::GpuEffectPassOutput::Type::Canvas) {
            outTarget = &canvas;
        } else {
            const auto it = buffers.find(pass.output.bufferId);
            if (it == buffers.end()) {
                failed = true;
                break;
            }
            outTarget = &it->second;
        }

        outTarget->fbo->bind();
        gl->glViewport(0, 0, outTarget->width, outTarget->height);
        gl->glDisable(GL_BLEND);
        gl->glClearColor(0.f, 0.f, 0.f, 0.f);
        gl->glClear(GL_COLOR_BUFFER_BIT);

        program->bind();
        setPackageUniforms(program, parameters, inputSize, timeUs, progress);

        // Bind all declared inputs: unit 0 → u_currentTexture, unit i → u_texture{i}.
        const QList<drift::GpuEffectPassInput> inputs =
            pass.inputs.isEmpty() ? QList<drift::GpuEffectPassInput>{drift::GpuEffectPassInput{}}
                                  : pass.inputs;
        int fromUnit = -1;
        int toUnit = -1;
        for (int texUnit = 0; texUnit < inputs.size(); ++texUnit) {
            const drift::GpuEffectPassInput &in = inputs[texUnit];
            GLuint tex = 0;
            switch (in.type) {
            case drift::GpuEffectPassInput::Type::SourceTexture:
                tex = sourceTexAt(in.sourceIndex);
                if (in.sourceIndex == 0)
                    fromUnit = texUnit;
                else if (in.sourceIndex == 1)
                    toUnit = texUnit;
                break;
            case drift::GpuEffectPassInput::Type::Buffer: {
                const auto it = buffers.find(in.bufferId);
                if (it == buffers.end()) {
                    failed = true;
                    break;
                }
                tex = it->second.texture();
                if (texUnit == 0)
                    inputSize = QSize(it->second.width, it->second.height);
                break;
            }
            case drift::GpuEffectPassInput::Type::Texture: {
                const auto it = textures.find(in.textureId);
                tex = it == textures.end() ? 0 : it->second;
                break;
            }
            }
            if (failed)
                break;

            gl->glActiveTexture(GL_TEXTURE0 + texUnit);
            gl->glBindTexture(GL_TEXTURE_2D, tex);
            if (texUnit == 0)
                program->setUniformValue("u_currentTexture", 0);
            else
                program->setUniformValue(qPrintable(QStringLiteral("u_texture%1").arg(texUnit)), texUnit);
        }

        if (!failed) {
            // Transition-friendly aliases pointing at whichever units hold source 0 and source 1.
            // uniformLocation() returns -1 for names a shader does not declare, so this is free.
            if (fromUnit >= 0)
                program->setUniformValue("u_fromTexture", fromUnit);
            if (toUnit >= 0)
                program->setUniformValue("u_toTexture", toUnit);

            // Re-apply resolution after possible buffer-sized primary input.
            program->setUniformValue("u_resolution",
                                     QVector2D(float(inputSize.width()), float(inputSize.height())));

            gl->glBindVertexArray(rt.vao);
            gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            gl->glBindVertexArray(0);
        }

        program->release();
        outTarget->fbo->release();
    }

    for (auto &entry : buffers)
        rt.releaseTarget(std::move(entry.second));
    buffers.clear();

    if (failed) {
        qWarning("GlRuntime: pass failed for %s — passthrough", qPrintable(cacheKey));
        rt.releaseTarget(std::move(canvas));
        return {};
    }
    return canvas;
}

} // namespace drift::gl
