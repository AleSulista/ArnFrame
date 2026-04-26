#include "TimelineExporter.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>

namespace {

QString ffmpegExecutable()
{
    const QString found = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    return found.isEmpty() ? QStringLiteral("ffmpeg") : found;
}

bool runFfmpeg(const QStringList &args, QString *errorOut)
{
    QProcess process;
    process.start(ffmpegExecutable(), args);
    if (!process.waitForStarted(5000)) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to start ffmpeg");
        return false;
    }
    if (!process.waitForFinished(-1)) {
        if (errorOut)
            *errorOut = QStringLiteral("ffmpeg timed out");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut)
            *errorOut = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return false;
    }
    return true;
}

QStringList baseOutputArgs(const QString &outputPath)
{
    return {
        QStringLiteral("-c:v"), QStringLiteral("libx264"),
        QStringLiteral("-preset"), QStringLiteral("fast"),
        QStringLiteral("-crf"), QStringLiteral("23"),
        QStringLiteral("-c:a"), QStringLiteral("aac"),
        QStringLiteral("-movflags"), QStringLiteral("+faststart"),
        QStringLiteral("-y"),
        outputPath,
    };
}

} // namespace

bool TimelineExporter::exportVideo(const QList<ExportSegment> &segments, const QString &outputPath,
                                   QString *errorOut)
{
    if (segments.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No clips to export");
        return false;
    }

    QList<ExportSegment> ordered = segments;
    std::stable_sort(ordered.begin(), ordered.end(), [](const ExportSegment &a, const ExportSegment &b) {
        return a.timelineStart < b.timelineStart;
    });

    if (ordered.size() == 1) {
        const ExportSegment &segment = ordered.front();
        QStringList args;
        args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("error");

        if (segment.kind == QStringLiteral("image")) {
            args << QStringLiteral("-loop") << QStringLiteral("1")
                 << QStringLiteral("-i") << segment.path
                 << QStringLiteral("-t") << QString::number(segment.duration, 'f', 3);
        } else {
            args << QStringLiteral("-ss") << QString::number(segment.inPoint, 'f', 3)
                 << QStringLiteral("-i") << segment.path
                 << QStringLiteral("-t") << QString::number(segment.duration, 'f', 3);
        }

        args << baseOutputArgs(outputPath);
        return runFfmpeg(args, errorOut);
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to create temporary directory");
        return false;
    }

    QStringList segmentPaths;
    segmentPaths.reserve(ordered.size());

    for (int i = 0; i < ordered.size(); ++i) {
        const ExportSegment &segment = ordered.at(i);
        const QString segmentPath = tempDir.filePath(QStringLiteral("segment_%1.mp4").arg(i));

        QStringList args;
        args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("error");

        if (segment.kind == QStringLiteral("image")) {
            args << QStringLiteral("-loop") << QStringLiteral("1")
                 << QStringLiteral("-i") << segment.path
                 << QStringLiteral("-t") << QString::number(segment.duration, 'f', 3);
        } else {
            args << QStringLiteral("-ss") << QString::number(segment.inPoint, 'f', 3)
                 << QStringLiteral("-i") << segment.path
                 << QStringLiteral("-t") << QString::number(segment.duration, 'f', 3);
        }

        args << baseOutputArgs(segmentPath);

        if (!runFfmpeg(args, errorOut))
            return false;

        segmentPaths.append(segmentPath);
    }

    const QString listPath = tempDir.filePath(QStringLiteral("concat.txt"));
    QFile listFile(listPath);
    if (!listFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to write concat list");
        return false;
    }

    for (const QString &segmentPath : segmentPaths)
        listFile.write(QStringLiteral("file '%1'\n").arg(segmentPath).toUtf8());
    listFile.close();

    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("error")
         << QStringLiteral("-f") << QStringLiteral("concat")
         << QStringLiteral("-safe") << QStringLiteral("0")
         << QStringLiteral("-i") << listPath
         << QStringLiteral("-c") << QStringLiteral("copy")
         << QStringLiteral("-y")
         << outputPath;

    return runFfmpeg(args, errorOut);
}
