#pragma once

#include <QList>
#include <QString>

namespace drift {

// Reserved uniform names supplied by the engine — package JSON must not claim these.
inline bool isReservedGpuUniform(const QString &name)
{
    if (name == QLatin1String("u_resolution") || name == QLatin1String("u_time")
        || name == QLatin1String("u_timeUs") || name == QLatin1String("u_frameIndex")
        || name == QLatin1String("u_currentTexture") || name == QLatin1String("u_progress")
        || name == QLatin1String("u_fromTexture") || name == QLatin1String("u_toTexture")) {
        return true;
    }
    // Extra samplers bound for multi-input passes: u_texture1, u_texture2, ...
    return name.startsWith(QLatin1String("u_texture"));
}

struct GpuEffectBufferSpec
{
    QString id;
    double scale = 1.0;
};

// Static image asset loaded from the package dir and bound as a sampler.
struct GpuEffectTextureSpec
{
    QString id;
    QString file; // relative to packageDir
    QString path; // resolved absolute path
};

struct GpuEffectPassInput
{
    enum class Type { SourceTexture, Buffer, Texture };
    Type type = Type::SourceTexture;
    QString bufferId;    // Buffer
    QString textureId;   // Texture
    int sourceIndex = 0; // SourceTexture: 0 = from/primary, 1 = to, ...
};

struct GpuEffectPassOutput
{
    enum class Type { Buffer, Canvas };
    Type type = Type::Canvas;
    QString bufferId;
};

struct GpuEffectPass
{
    int passIndex = 0;
    QString fragmentShaderFile;   // relative filename from the package JSON
    QString fragmentShaderSource; // loaded GLSL
    QList<GpuEffectPassInput> inputs;
    GpuEffectPassOutput output;
};

// Parsed, validated GPU package pipeline (effect.json / transition.json + shaders).
struct GpuEffectDefinition
{
    QString packageDir;
    QList<GpuEffectBufferSpec> intermediateBuffers;
    QList<GpuEffectTextureSpec> textures;
    QList<GpuEffectPass> passes;
    bool valid = false;
    QString errorMessage;
};

} // namespace drift
