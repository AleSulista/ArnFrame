#include "TimelineExporter.h"

#include <QFile>
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

QStringList encodeOutputArgs(const QString &outputPath)
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

QStringList inputArgsForSegment(const ExportSegment &segment)
{
    QStringList args;
    if (segment.kind == QStringLiteral("image")) {
        args << QStringLiteral("-loop") << QStringLiteral("1")
             << QStringLiteral("-i") << segment.path
             << QStringLiteral("-t") << QString::number(segment.duration, 'f', 3);
    } else {
        args << QStringLiteral("-ss") << QString::number(segment.inPoint, 'f', 3)
             << QStringLiteral("-i") << segment.path
             << QStringLiteral("-t") << QString::number(segment.duration, 'f', 3);
    }
    return args;
}

QList<ExportSegment> sortedSegments(QList<ExportSegment> segments)
{
    std::stable_sort(segments.begin(), segments.end(), [](const ExportSegment &a, const ExportSegment &b) {
        return a.timelineStart < b.timelineStart;
    });
    return segments;
}

bool exportSegmentList(const QList<ExportSegment> &segments, const QString &outputPath, bool videoOnly,
                       QString *errorOut)
{
    if (segments.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No segments to export");
        return false;
    }

    const QList<ExportSegment> ordered = sortedSegments(segments);

    if (ordered.size() == 1) {
        QStringList args;
        args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("error");
        args << inputArgsForSegment(ordered.front());
        if (videoOnly)
            args << QStringLiteral("-an");
        args << encodeOutputArgs(outputPath);
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
        const QString segmentPath = tempDir.filePath(QStringLiteral("segment_%1.mp4").arg(i));
        QStringList args;
        args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("error");
        args << inputArgsForSegment(ordered.at(i));
        if (videoOnly)
            args << QStringLiteral("-an");
        args << encodeOutputArgs(segmentPath);

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

bool muxAudioOverlay(const QString &videoPath, const QList<ExportSegment> &audioSegments,
                     const QString &outputPath, QString *errorOut)
{
    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("error")
         << QStringLiteral("-i") << videoPath;

    QStringList filterParts;
    QStringList mixInputs;

    for (int i = 0; i < audioSegments.size(); ++i) {
        const ExportSegment &segment = audioSegments.at(i);
        args << QStringLiteral("-ss") << QString::number(segment.inPoint, 'f', 3)
             << QStringLiteral("-i") << segment.path
             << QStringLiteral("-t") << QString::number(segment.duration, 'f', 3);

        const int inputIndex = i + 1;
        const int delayMs = static_cast<int>(segment.timelineStart * 1000.0);
        const QString label = QStringLiteral("a%1").arg(i);
        filterParts.append(QStringLiteral("[%1:a]asetpts=PTS-STARTPTS,adelay=%2|%2[%3]")
                               .arg(inputIndex)
                               .arg(delayMs)
                               .arg(delayMs)
                               .arg(label));
        mixInputs.append(QStringLiteral("[%1]").arg(label));
    }

    if (mixInputs.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No audio segments to mix");
        return false;
    }

    const QString filter = filterParts.join(QLatin1Char(';'))
                           + QStringLiteral(";")
                           + mixInputs.join(QString())
                           + QStringLiteral("amix=inputs=%1:duration=longest:dropout_transition=0[aout]")
                                 .arg(mixInputs.size());

    args << QStringLiteral("-filter_complex") << filter
         << QStringLiteral("-map") << QStringLiteral("0:v:0")
         << QStringLiteral("-map") << QStringLiteral("[aout]")
         << QStringLiteral("-c:v") << QStringLiteral("copy")
         << QStringLiteral("-c:a") << QStringLiteral("aac")
         << QStringLiteral("-movflags") << QStringLiteral("+faststart")
         << QStringLiteral("-shortest")
         << QStringLiteral("-y")
         << outputPath;

    return runFfmpeg(args, errorOut);
}

} // namespace

bool TimelineExporter::exportTimeline(const QList<ExportSegment> &videoSegments,
                                      const QList<ExportSegment> &audioSegments,
                                      const QString &outputPath, QString *errorOut)
{
    if (videoSegments.isEmpty() && audioSegments.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No clips to export");
        return false;
    }

    if (videoSegments.isEmpty()) {
        return exportSegmentList(audioSegments, outputPath, false, errorOut);
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to create temporary directory");
        return false;
    }

    const QString videoPath = tempDir.filePath(QStringLiteral("video.mp4"));
    const bool videoOnly = !audioSegments.isEmpty();
    if (!exportSegmentList(videoSegments, videoPath, videoOnly, errorOut))
        return false;

    if (audioSegments.isEmpty()) {
        if (QFile::exists(outputPath))
            QFile::remove(outputPath);
        return QFile::copy(videoPath, outputPath);
    }

    return muxAudioOverlay(videoPath, sortedSegments(audioSegments), outputPath, errorOut);
}
