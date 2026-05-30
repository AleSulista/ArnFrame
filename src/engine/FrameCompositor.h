#pragma once

#include "GpuCompositor.h"
#include "core/Project.h"
#include "core/Time.h"

#include <QImage>

// Composites all visible tracks into a single RGBA frame at timeline time T.
class FrameCompositor
{
public:
    struct RenderOptions
    {
        double previewScale = 1.0;
        int maxTimeEchoHistoryFrames = -1;
    };

    void setProject(const drift::Project *project) { m_project = project; }

    QImage compositeAt(drift::TimeUs timelineUs) const;
    QImage compositeAt(drift::TimeUs timelineUs, const RenderOptions &options) const;

    // Composite and leave the frame on the GPU. Returns an invalid handle when
    // OpenGL is unavailable, in which case callers should use compositeAt.
    GpuFrameTexture compositeToTextureAt(drift::TimeUs timelineUs, const RenderOptions &options) const;

private:
    // Shared by both entry points: resolves the canvas size, warms the decoders
    // and builds the scene. Returns false when there is nothing to render.
    bool prepare(drift::TimeUs timelineUs, const RenderOptions &options, GpuScene *sceneOut, int *widthOut,
                 int *heightOut, double *renderScaleOut) const;


    const drift::Project *m_project = nullptr;
};

Q_DECLARE_METATYPE(FrameCompositor::RenderOptions)
