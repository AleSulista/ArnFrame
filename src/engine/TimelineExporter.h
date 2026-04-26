#pragma once

#include <QList>
#include <QString>

struct ExportSegment {
    QString path;
    QString kind;
    double timelineStart = 0.0;
    double inPoint = 0.0;
    double duration = 0.0;
};

class TimelineExporter
{
public:
    static bool exportTimeline(const QList<ExportSegment> &videoSegments,
                               const QList<ExportSegment> &audioSegments,
                               const QString &outputPath,
                               QString *errorOut);
};
