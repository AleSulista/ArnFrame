#pragma once

#include <QList>
#include <QString>

namespace drift {

// Reserved uniform names supplied by the engine — effect.json must not claim these.
inline bool isReservedGpuUniform(const QString &name)
{
    if (name == QLatin1String("u_resolution") || name == QLatin1String("u_time")
        || name == QLatin1String("u_timeUs") || name == QLatin1String("u_frameIndex")
        || name == QLatin1String("u_currentTexture")) {
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

struct GpuEffectPassInput
{
    enum class Type { SourceTexture, Buffer };
    Type type = Type::SourceTexture;
    QString bufferId;
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
    QString fragmentShaderFile;   // relative filename from effect.json
    QString fragmentShaderSource; // loaded GLSL
    QList<GpuEffectPassInput> inputs;
    GpuEffectPassOutput output;
};

// Parsed, validated GPU effect package (effect.json + shaders).
struct GpuEffectDefinition
{
    QString packageDir;
    QList<GpuEffectBufferSpec> intermediateBuffers;
    QList<GpuEffectPass> passes;
    bool valid = false;
    QString errorMessage;
};

} // namespace drift
