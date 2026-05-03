#pragma once

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

private:
    const drift::Project *m_project = nullptr;
};

Q_DECLARE_METATYPE(FrameCompositor::RenderOptions)
