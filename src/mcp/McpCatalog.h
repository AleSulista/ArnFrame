#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace drift::mcp {

QStringList toolboxNames();
QJsonObject catalogPayload();
QJsonObject toolboxPayload(const QString &name);
QJsonArray homepageTools();
QJsonArray toolboxDirectTools(const QString &name);
bool isHomepageTool(const QString &name);
bool isKnownOp(const QString &name);
bool isReadOnlyOp(const QString &name);
QString toolboxForOp(const QString &name);
QString homepageHtml();
QString agentGuideText();
// Mutation ops that isUndoable skips, in addition to every read-only op. The catalog
// limitations, undo/apply descriptions and docs/MCP.md are generated from this list.
QStringList undoExemptOps();
// Ops that take no clip argument and act on the current selection. freeze_frame and
// paste_at_playhead are playhead-based and are deliberately not in this list.
QStringList selectionBasedOps();

} // namespace drift::mcp
