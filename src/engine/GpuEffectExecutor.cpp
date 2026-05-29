#include "GpuEffectExecutor.h"

#include "GpuEffectDefinition.h"

#include <QColor>
#include <QCoreApplication>
#include <QMetaType>
#include <QMutex>
#include <QMutexLocker>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>
#include <QVector2D>
#include <QVector3D>

#include <cmath>
#include <map>
#include <memory>
#include <vector>

namespace {

constexpr const char *kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texCoord;
out vec2 v_texCoord;
void main() {
    v_texCoord = a_texCoord;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

constexpr const char *kCopyFragShader = R"(#version 330 core
in vec2 v_texCoord;
out vec4 fragColor;
uniform sampler2D u_currentTexture;
void main() {
    fragColor = texture(u_currentTexture, v_texCoord);
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

struct GlTarget {
    std::unique_ptr<QOpenGLFramebufferObject> fbo;
    int width = 0;
    int height = 0;

    GlTarget() = default;
    GlTarget(GlTarget &&) noexcept = default;
    GlTarget &operator=(GlTarget &&) noexcept = default;
    GlTarget(const GlTarget &) = delete;
    GlTarget &operator=(const GlTarget &) = delete;
};

struct CompiledPass {
    std::unique_ptr<QOpenGLShaderProgram> program;

    CompiledPass() = default;
    CompiledPass(CompiledPass &&) noexcept = default;
    CompiledPass &operator=(CompiledPass &&) noexcept = default;
    CompiledPass(const CompiledPass &) = delete;
    CompiledPass &operator=(const CompiledPass &) = delete;
};

struct CompiledEffect {
    QString id;
    QString sourceSig;
    std::vector<CompiledPass> passes;
    bool ok = false;
};

class GlRuntime
{
public:
    QMutex mutex;
    std::unique_ptr<QOpenGLContext> context;
    std::unique_ptr<QOffscreenSurface> surface;
    bool ready = false;
    GLuint vao = 0;
    GLuint vbo = 0;
    std::unique_ptr<QOpenGLShaderProgram> copyProgram;
    std::map<QString, CompiledEffect> programs;

    bool init()
    {
        if (ready)
            return true;

        QSurfaceFormat format;
        format.setVersion(3, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setDepthBufferSize(0);
        format.setStencilBufferSize(0);

        surface = std::make_unique<QOffscreenSurface>();
        surface->setFormat(format);
        surface->create();
        if (!surface->isValid()) {
            qWarning("GpuEffectExecutor: failed to create offscreen surface");
            surface.reset();
            return false;
        }

        context = std::make_unique<QOpenGLContext>();
        context->setFormat(format);
        if (!context->create()) {
            qWarning("GpuEffectExecutor: failed to create OpenGL context");
            context.reset();
            surface.reset();
            return false;
        }
        if (!context->makeCurrent(surface.get())) {
            qWarning("GpuEffectExecutor: makeCurrent failed");
            context.reset();
            surface.reset();
            return false;
        }

        auto *gl = context->extraFunctions();
        if (!gl) {
            qWarning("GpuEffectExecutor: OpenGL extra functions unavailable");
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
        gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                                  reinterpret_cast<void *>(0));
        gl->glEnableVertexAttribArray(1);
        gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                                  reinterpret_cast<void *>(2 * sizeof(float)));
        gl->glBindVertexArray(0);

        copyProgram = std::make_unique<QOpenGLShaderProgram>();
        if (!copyProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)
            || !copyProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kCopyFragShader)
            || !copyProgram->link()) {
            qWarning("GpuEffectExecutor: copy shader failed: %s", qPrintable(copyProgram->log()));
            copyProgram.reset();
            context->doneCurrent();
            context.reset();
            surface.reset();
            return false;
        }

        ready = true;
        return true;
    }

    bool makeCurrent()
    {
        if (!ready && !init())
            return false;
        return context && surface && context->makeCurrent(surface.get());
    }

    void doneCurrent()
    {
        if (context)
            context->doneCurrent();
    }

    void shutdown()
    {
        if (!ready && !context)
            return;
        if (context && surface && context->makeCurrent(surface.get())) {
            programs.clear();
            copyProgram.reset();
            if (auto *gl = context->extraFunctions()) {
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
        } else {
            programs.clear();
        }
        context.reset();
        surface.reset();
        ready = false;
    }

    CompiledEffect *compile(const EffectPresetEntry &def)
    {
        QString sourceSig;
        for (const drift::GpuEffectPass &pass : def.gpu.passes)
            sourceSig += pass.fragmentShaderSource;
        sourceSig += QLatin1Char('#');
        sourceSig += QString::number(def.gpu.passes.size());

        CompiledEffect &cached = programs[def.meta.id];
        if (cached.ok && cached.id == def.meta.id && cached.sourceSig == sourceSig)
            return &cached;

        cached = CompiledEffect{};
        cached.id = def.meta.id;
        cached.sourceSig = sourceSig;
        cached.passes.reserve(static_cast<size_t>(def.gpu.passes.size()));

        for (const drift::GpuEffectPass &pass : def.gpu.passes) {
            CompiledPass cp;
            cp.program = std::make_unique<QOpenGLShaderProgram>();
            if (!cp.program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)) {
                qWarning("GpuEffectExecutor: vertex shader compile failed for %s: %s",
                         qPrintable(def.meta.id), qPrintable(cp.program->log()));
                programs.erase(def.meta.id);
                return nullptr;
            }
            if (!cp.program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                     pass.fragmentShaderSource)) {
                qWarning("GpuEffectExecutor: fragment compile failed for %s pass %d (%s): %s",
                         qPrintable(def.meta.id), pass.passIndex,
                         qPrintable(pass.fragmentShaderFile), qPrintable(cp.program->log()));
                programs.erase(def.meta.id);
                return nullptr;
            }
            if (!cp.program->link()) {
                qWarning("GpuEffectExecutor: link failed for %s pass %d: %s",
                         qPrintable(def.meta.id), pass.passIndex, qPrintable(cp.program->log()));
                programs.erase(def.meta.id);
                return nullptr;
            }
            cached.passes.push_back(std::move(cp));
        }
        cached.ok = true;
        return &cached;
    }
};

GlRuntime *g_runtime = nullptr;

void destroyGpuRuntime()
{
    if (!g_runtime)
        return;
    {
        QMutexLocker lock(&g_runtime->mutex);
        g_runtime->shutdown();
    }
    delete g_runtime;
    g_runtime = nullptr;
}

GlRuntime &runtime()
{
    if (!g_runtime) {
        g_runtime = new GlRuntime;
        qAddPostRoutine(destroyGpuRuntime);
    }
    return *g_runtime;
}

GLuint uploadTexture(QOpenGLExtraFunctions *gl, const QImage &image)
{
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    GLuint tex = 0;
    gl->glGenTextures(1, &tex);
    gl->glBindTexture(GL_TEXTURE_2D, tex);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.width(), rgba.height(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, rgba.constBits());
    return tex;
}

bool blitTextureToTarget(GlRuntime &rt, QOpenGLExtraFunctions *gl, GLuint srcTex, GlTarget &dest)
{
    if (!rt.copyProgram || !dest.fbo || !dest.fbo->isValid())
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

GlTarget makeTarget(int width, int height)
{
    GlTarget t;
    t.width = qMax(1, width);
    t.height = qMax(1, height);
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    t.fbo = std::make_unique<QOpenGLFramebufferObject>(t.width, t.height, fmt);
    return t;
}

void setUniforms(QOpenGLShaderProgram *program, const QMap<QString, QVariant> &parameters,
                 const QSize &resolution, drift::TimeUs timeUs)
{
    program->setUniformValue("u_currentTexture", 0);
    program->setUniformValue("u_resolution", QVector2D(float(resolution.width()),
                                                       float(resolution.height())));
    const float timeSec = float(timeUs) / 1'000'000.f;
    program->setUniformValue("u_time", timeSec);
    program->setUniformValue("u_frameIndex", int(std::floor(timeSec * 30.f)));
    // Integer microseconds for hash-stable glitch effects that mirror CPU seeding.
    program->setUniformValue("u_timeUs", float(timeUs));

    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        if (drift::isReservedGpuUniform(it.key()))
            continue;
        const int loc = program->uniformLocation(it.key());
        if (loc < 0)
            continue;
        const QVariant &value = it.value();
        if (value.typeId() == QMetaType::Bool) {
            program->setUniformValue(loc, value.toBool() ? 1.f : 0.f);
        } else if (value.typeId() == QMetaType::QString) {
            const QString s = value.toString();
            if (s.startsWith(QLatin1Char('#'))) {
                const QColor c(s);
                program->setUniformValue(loc, QVector3D(float(c.redF()), float(c.greenF()),
                                                        float(c.blueF())));
            } else if (it.key() == QLatin1String("position")
                       || it.key() == QLatin1String("blendMode")) {
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

} // namespace

GpuEffectExecutor &GpuEffectExecutor::instance()
{
    static GpuEffectExecutor exec;
    return exec;
}

bool GpuEffectExecutor::ensureContext()
{
    if (m_triedInit)
        return m_available;
    m_triedInit = true;

    GlRuntime &rt = runtime();
    QMutexLocker lock(&rt.mutex);
    m_available = rt.init();
    if (m_available)
        rt.doneCurrent();
    return m_available;
}

bool GpuEffectExecutor::isAvailable()
{
    return ensureContext();
}

void GpuEffectExecutor::releaseGl()
{
    // Kept for future shutdown hooks; context lives for process lifetime.
}

QImage GpuEffectExecutor::apply(const EffectPresetEntry &def, const QImage &input,
                                const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    if (input.isNull() || !def.isGpu || !def.gpu.valid)
        return input;
    if (!ensureContext())
        return input;

    GlRuntime &rt = runtime();
    QMutexLocker lock(&rt.mutex);
    if (!rt.makeCurrent()) {
        qWarning("GpuEffectExecutor: makeCurrent failed for %s", qPrintable(def.meta.id));
        return input;
    }

    CompiledEffect *compiled = rt.compile(def);
    if (!compiled || !compiled->ok || compiled->passes.size() != def.gpu.passes.size()) {
        rt.doneCurrent();
        return input;
    }

    auto *gl = rt.context->extraFunctions();
    if (!gl) {
        rt.doneCurrent();
        return input;
    }
    const QImage srcImage = input.convertToFormat(QImage::Format_RGBA8888);
    const GLuint uploaded = uploadTexture(gl, srcImage);

    // Promote the QImage upload into an FBO so source and intermediate buffers share
    // one texture orientation (fixes bloom/glow compositing upside-down vs video).
    GlTarget sourceTarget = makeTarget(srcImage.width(), srcImage.height());
    if (!sourceTarget.fbo || !sourceTarget.fbo->isValid()
        || !blitTextureToTarget(rt, gl, uploaded, sourceTarget)) {
        qWarning("GpuEffectExecutor: source FBO promote failed for %s", qPrintable(def.meta.id));
        gl->glDeleteTextures(1, &uploaded);
        rt.doneCurrent();
        return input;
    }
    gl->glDeleteTextures(1, &uploaded);
    const GLuint sourceTex = sourceTarget.fbo->texture();

    std::map<QString, GlTarget> buffers;
    for (const drift::GpuEffectBufferSpec &spec : def.gpu.intermediateBuffers) {
        const int w = qMax(1, int(std::lround(srcImage.width() * spec.scale)));
        const int h = qMax(1, int(std::lround(srcImage.height() * spec.scale)));
        buffers.emplace(spec.id, makeTarget(w, h));
        GlTarget &target = buffers.at(spec.id);
        if (!target.fbo || !target.fbo->isValid()) {
            qWarning("GpuEffectExecutor: FBO alloc failed for buffer %s", qPrintable(spec.id));
            buffers.clear();
            sourceTarget.fbo.reset();
            rt.doneCurrent();
            return input;
        }
    }

    GlTarget canvas = makeTarget(srcImage.width(), srcImage.height());
    if (!canvas.fbo || !canvas.fbo->isValid()) {
        qWarning("GpuEffectExecutor: canvas FBO alloc failed");
        buffers.clear();
        sourceTarget.fbo.reset();
        rt.doneCurrent();
        return input;
    }

    bool failed = false;
    for (int i = 0; i < def.gpu.passes.size(); ++i) {
        const drift::GpuEffectPass &pass = def.gpu.passes[i];
        QOpenGLShaderProgram *program = compiled->passes[static_cast<size_t>(i)].program.get();

        QSize inputSize = srcImage.size();

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
        if (!outTarget || !outTarget->fbo) {
            failed = true;
            break;
        }

        outTarget->fbo->bind();
        gl->glViewport(0, 0, outTarget->width, outTarget->height);
        gl->glDisable(GL_BLEND);
        gl->glClearColor(0.f, 0.f, 0.f, 0.f);
        gl->glClear(GL_COLOR_BUFFER_BIT);

        program->bind();
        setUniforms(program, parameters, inputSize, timeUs);

        // Bind all declared inputs: unit 0 → u_currentTexture, unit i → u_texture{i}.
        const QList<drift::GpuEffectPassInput> inputs =
            pass.inputs.isEmpty()
                ? QList<drift::GpuEffectPassInput>{{drift::GpuEffectPassInput::Type::SourceTexture, {}}}
                : pass.inputs;
        for (int texUnit = 0; texUnit < inputs.size(); ++texUnit) {
            const drift::GpuEffectPassInput &in = inputs[texUnit];
            GLuint tex = sourceTex;
            if (in.type == drift::GpuEffectPassInput::Type::Buffer) {
                const auto it = buffers.find(in.bufferId);
                if (it == buffers.end() || !it->second.fbo) {
                    failed = true;
                    break;
                }
                tex = it->second.fbo->texture();
                if (texUnit == 0)
                    inputSize = QSize(it->second.width, it->second.height);
            }
            gl->glActiveTexture(GL_TEXTURE0 + texUnit);
            gl->glBindTexture(GL_TEXTURE_2D, tex);
            if (texUnit == 0) {
                program->setUniformValue("u_currentTexture", 0);
            } else {
                program->setUniformValue(qPrintable(QStringLiteral("u_texture%1").arg(texUnit)), texUnit);
            }
        }
        if (failed)
            break;

        // Re-apply resolution after possible buffer-sized primary input.
        program->setUniformValue("u_resolution", QVector2D(float(inputSize.width()),
                                                           float(inputSize.height())));

        gl->glBindVertexArray(rt.vao);
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl->glBindVertexArray(0);

        program->release();
        outTarget->fbo->release();
    }

    QImage result = input;
    if (!failed) {
        // Source was promoted into an FBO; all samples share that Y layout.
        // toImage(false) keeps QImage top matching the original frame top.
        result = canvas.fbo->toImage(false).convertToFormat(QImage::Format_RGBA8888);
        if (result.size() != srcImage.size())
            result = result.scaled(srcImage.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    } else {
        qWarning("GpuEffectExecutor: pass failed for %s — passthrough", qPrintable(def.meta.id));
    }

    // FBO textures are owned by QOpenGLFramebufferObject — do not glDelete them.
    buffers.clear();
    sourceTarget.fbo.reset();
    canvas.fbo.reset();
    rt.doneCurrent();
    return result;
}

QImage GpuEffectExecutor::blendTimeEcho(const QList<QImage> &framesNewestFirst, double decay,
                                        int blendMode)
{
    if (framesNewestFirst.isEmpty() || !ensureContext())
        return {};

    GlRuntime &rt = runtime();
    QMutexLocker lock(&rt.mutex);
    if (!rt.makeCurrent())
        return {};

    auto *gl = rt.context->extraFunctions();
    if (!gl) {
        rt.doneCurrent();
        return {};
    }

    static const char *kBlendFrag = R"(#version 330 core
in vec2 v_texCoord;
out vec4 fragColor;
uniform sampler2D u_currentTexture; // overlay (newer)
uniform sampler2D u_texture1;       // base (accumulated)
uniform float opacity;
uniform float blendMode; // 0 normal, 1 add, 2 screen
void main() {
    vec4 over = texture(u_currentTexture, v_texCoord);
    vec4 base = texture(u_texture1, v_texCoord);
    float a = clamp(opacity, 0.0, 1.0) * over.a;
    vec3 rgb;
    if (blendMode > 1.5) {
        // screen
        rgb = 1.0 - (1.0 - base.rgb) * (1.0 - over.rgb);
    } else if (blendMode > 0.5) {
        rgb = base.rgb + over.rgb;
    } else {
        rgb = mix(base.rgb, over.rgb, a);
    }
    float outA = clamp(base.a + a, 0.0, 1.0);
    fragColor = vec4(clamp(rgb, 0.0, 1.0), outA);
}
)";

    // Compile once into a dedicated program entry.
    const QString cacheId = QStringLiteral("__time_echo_blend__");
    CompiledEffect &cached = rt.programs[cacheId];
    if (!cached.ok) {
        cached = CompiledEffect{};
        cached.id = cacheId;
        CompiledPass cp;
        cp.program = std::make_unique<QOpenGLShaderProgram>();
        if (!cp.program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)
            || !cp.program->addShaderFromSourceCode(QOpenGLShader::Fragment, kBlendFrag)
            || !cp.program->link()) {
            qWarning("GpuEffectExecutor: time_echo blend shader failed: %s",
                     qPrintable(cp.program->log()));
            rt.programs.erase(cacheId);
            rt.doneCurrent();
            return {};
        }
        cached.passes.push_back(std::move(cp));
        cached.ok = true;
    }

    QOpenGLShaderProgram *program = cached.passes[0].program.get();
    const double clampedDecay = qBound(0.0, decay, 1.0);
    QImage result;

    // Oldest → newest, matching CPU applyTimeEcho.
    for (int age = framesNewestFirst.size() - 1; age >= 0; --age) {
        const QImage layer = framesNewestFirst.at(age).convertToFormat(QImage::Format_RGBA8888);
        if (layer.isNull())
            continue;
        const double opacity = age == 0 ? 1.0 : std::pow(clampedDecay, static_cast<double>(age));

        if (result.isNull()) {
            result = layer;
            continue;
        }
        if (result.size() != layer.size())
            continue;

        const GLuint overTex = uploadTexture(gl, layer);
        const GLuint baseTex = uploadTexture(gl, result);
        GlTarget canvas = makeTarget(layer.width(), layer.height());
        if (!canvas.fbo || !canvas.fbo->isValid()) {
            gl->glDeleteTextures(1, &overTex);
            gl->glDeleteTextures(1, &baseTex);
            rt.doneCurrent();
            return {};
        }

        canvas.fbo->bind();
        gl->glViewport(0, 0, canvas.width, canvas.height);
        gl->glDisable(GL_BLEND);
        gl->glClearColor(0.f, 0.f, 0.f, 0.f);
        gl->glClear(GL_COLOR_BUFFER_BIT);

        program->bind();
        program->setUniformValue("u_currentTexture", 0);
        program->setUniformValue("u_texture1", 1);
        program->setUniformValue("opacity", float(opacity));
        program->setUniformValue("blendMode", float(blendMode));
        gl->glActiveTexture(GL_TEXTURE0);
        gl->glBindTexture(GL_TEXTURE_2D, overTex);
        gl->glActiveTexture(GL_TEXTURE1);
        gl->glBindTexture(GL_TEXTURE_2D, baseTex);
        gl->glBindVertexArray(rt.vao);
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl->glBindVertexArray(0);
        program->release();
        canvas.fbo->release();

        result = canvas.fbo->toImage(false).convertToFormat(QImage::Format_RGBA8888);
        gl->glDeleteTextures(1, &overTex);
        gl->glDeleteTextures(1, &baseTex);
    }

    rt.doneCurrent();
    return result;
}
