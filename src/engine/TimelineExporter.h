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
    static bool exportVideo(const QList<ExportSegment> &segments, const QString &outputPath, QString *errorOut);
};
