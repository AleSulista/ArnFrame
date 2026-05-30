#pragma once

// Internal to the engine's GL code. Owns the process-wide offscreen OpenGL
// context, the compiled-program cache, the framebuffer pool and the static
// texture cache, and runs one GPU package's passes.
//
// Shared by GpuEffectExecutor (effects/transitions as QImage -> QImage) and
// GpuCompositor (whole-frame compositing that never leaves the GPU). Not a
// public API — do not include from outside src/engine.

#include "GpuEffectDefinition.h"
#include "core/Time.h"

#include <QImage>
#include <QMap>
#include <QMutex>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QSize>
#include <QString>
#include <QVariant>

#include <QThread>

#include <functional>
#include <map>
#include <memory>
#include <vector>

class QOffscreenSurface;
class QOpenGLContext;

namespace drift::gl {

// A framebuffer plus its size. Owns the FBO; hand it back to GlRuntime with
// releaseTarget() so it can be recycled rather than freed.
struct GlTarget
{
    std::unique_ptr<QOpenGLFramebufferObject> fbo;
    int width = 0;
    int height = 0;

    GlTarget() = default;
    GlTarget(GlTarget &&) noexcept = default;
    GlTarget &operator=(GlTarget &&) noexcept = default;
    GlTarget(const GlTarget &) = delete;
    GlTarget &operator=(const GlTarget &) = delete;

    bool isValid() const { return fbo && fbo->isValid(); }
    GLuint texture() const { return fbo ? fbo->texture() : 0; }
    QSize size() const { return QSize(width, height); }
};

struct CompiledPass
{
    std::unique_ptr<QOpenGLShaderProgram> program;

    CompiledPass() = default;
    CompiledPass(CompiledPass &&) noexcept = default;
    CompiledPass &operator=(CompiledPass &&) noexcept = default;
    CompiledPass(const CompiledPass &) = delete;
    CompiledPass &operator=(const CompiledPass &) = delete;
};

struct CompiledEffect
{
    QString id;
    QString sourceSig;
    std::vector<CompiledPass> passes;
    bool ok = false;
};

class GlRuntime
{
public:
    std::unique_ptr<QOpenGLContext> context;
    std::unique_ptr<QOffscreenSurface> surface;
    GLuint vao = 0;
    GLuint vbo = 0;
    std::unique_ptr<QOpenGLShaderProgram> copyProgram;
    std::map<QString, CompiledEffect> programs;
    std::map<QString, GLuint> staticTextures; // absolute path -> GL texture

    // A QOpenGLContext has thread affinity and can only be made current on the
    // thread it lives on, but GL work arrives from several threads (the
    // compositor worker, the export job, tools and tests). So the context lives
    // on one dedicated GL thread and every caller hands it a job to run there.
    // This also serializes GL, which is why there is no mutex around the context.
    //
    // Runs fn on the GL thread with the context current. Returns false if OpenGL
    // is unavailable, in which case fn does not run.
    bool exec(const std::function<void()> &fn);

    // True when a context could be created. Safe to call from any thread.
    bool available();

    QOpenGLExtraFunctions *functions();

    // Compiled programs are cached by key + source signature.
    CompiledEffect *compile(const QString &cacheKey, const drift::GpuEffectDefinition &gpu);

    // Framebuffers are recycled by size: allocating a fresh FBO per effect per
    // frame churns GPU memory during steady-state playback.
    GlTarget acquireTarget(int width, int height);
    void releaseTarget(GlTarget &&target);

    // Presentation ring. The preview's composited frame is handed to the Qt Quick
    // scene graph as a live GL texture rather than read back, so the target it
    // lives in cannot go back to the general pool while the scene graph samples
    // it. Rotating through a few targets gives the scene graph time to finish
    // with one before it is drawn into again.
    GlTarget &acquirePresentTarget(int width, int height);

    // Compile (once) and return an arbitrary fragment shader program, keyed by id.
    // Used for the compositor's own shaders, which are not package-defined.
    QOpenGLShaderProgram *builtinProgram(const QString &id, const char *vertexSource,
                                         const char *fragmentSource);

    // Tear down GL objects and stop the GL thread. Called at app exit.
    void shutdown();

private:
    bool ensureReady();
    bool initGlObjects();

    QMutex m_initMutex;
    bool m_initTried = false;
    bool m_ok = false;
    QThread *m_glThread = nullptr;
    QObject *m_glOwner = nullptr; // lives on m_glThread; the invoke target

    std::multimap<uint64_t, std::unique_ptr<QOpenGLFramebufferObject>> m_targetPool;
    size_t m_pooledTargets = 0;
    static constexpr size_t kMaxPooledTargets = 32;

    static constexpr int kPresentRingSize = 3;
    GlTarget m_presentRing[kPresentRingSize];
    int m_presentNext = 0;
};

GlRuntime &runtime();

// The fullscreen-quad vertex shader every package pass uses.
extern const char *const kQuadVertexShader;

GLuint uploadTexture(QOpenGLExtraFunctions *gl, const QImage &image, bool flipVertically = false);
bool blitTextureToTarget(GlRuntime &rt, QOpenGLExtraFunctions *gl, GLuint srcTex, GlTarget &dest);
GLuint staticTexture(GlRuntime &rt, QOpenGLExtraFunctions *gl, const QString &path);

// Upload a QImage into a pooled FBO, so sources, intermediate buffers and the
// canvas all share one texture orientation. A null image becomes transparent
// black at fallbackSize.
GlTarget promoteImageToTarget(GlRuntime &rt, QOpenGLExtraFunctions *gl, const QImage &image,
                              const QSize &fallbackSize);

void setPackageUniforms(QOpenGLShaderProgram *program, const QMap<QString, QVariant> &parameters,
                        const QSize &resolution, drift::TimeUs timeUs, double progress);

// Run every pass of one GPU package, reading from `sources` and returning a new
// pooled target with the result. Nothing is read back to the CPU. Returns an
// invalid target on failure (grace mode).
GlTarget runPipeline(GlRuntime &rt, QOpenGLExtraFunctions *gl, const QString &cacheKey,
                     const drift::GpuEffectDefinition &gpu, const std::vector<const GlTarget *> &sources,
                     const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs, double progress,
                     const QSize &canvasSize);

} // namespace drift::gl
