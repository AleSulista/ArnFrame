#pragma once

#include "core/Project.h"
#include "core/Time.h"

#include <QImage>

// Composites all visible tracks into a single RGBA frame at timeline time T.
class FrameCompositor
{
public:
    void setProject(const drift::Project *project) { m_project = project; }

    QImage compositeAt(drift::TimeUs timelineUs) const;

private:
    const drift::Project *m_project = nullptr;
};
