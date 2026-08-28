#pragma once

#include "core/Project.h"

#include <QUndoCommand>

namespace drift {

// Restores a full project snapshot on undo/redo.
class ProjectSnapshotCommand : public QUndoCommand
{
public:
    ProjectSnapshotCommand(Project *project, Project before, Project after, const QString &text);

    void undo() override;
    void redo() override;

    QString beforeHash() const { return m_beforeHash; }
    QString afterHash() const { return m_afterHash; }
    const Project &before() const { return m_before; }
    const Project &after() const { return m_after; }

private:
    Project *m_project = nullptr;
    Project m_before;
    Project m_after;
    QString m_beforeHash;
    QString m_afterHash;
};

} // namespace drift
