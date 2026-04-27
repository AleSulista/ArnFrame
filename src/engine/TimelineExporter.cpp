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

QStringList encodeVideoArgs(const QString &outputPath)
{
    return {
        QStringLiteral("-c:v"), QStringLiteral("libx264"),
        QStringLiteral("-preset"), QStringLiteral("fast"),
        QStringLiteral("-crf"), QStringLiteral("23"),
        QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
        QStringLiteral("-an"),
        QStringLiteral("-y"),
        outputPath,
    };
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

struct TimelinePiece {
    bool isGap = false;
    double duration = 0.0;
    ExportSegment segment;
};

QList<TimelinePiece> buildTimelinePieces(const QList<ExportSegment> &segments)
{
    const QList<ExportSegment> ordered = sortedSegments(segments);
    QList<TimelinePiece> pieces;
    double cursor = 0.0;

    for (const ExportSegment &segment : ordered) {
        if (segment.timelineStart > cursor + 0.001) {
            pieces.append({true, segment.timelineStart - cursor, {}});
            cursor = segment.timelineStart;
        }
        pieces.append({false, segment.duration, segment});
        cursor = segment.timelineStart + segment.duration;
    }

    return pieces;
}

bool exportBlackGap(double duration, const QString &outputPath, QString *errorOut)
{
    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("error")
         << QStringLiteral("-f") << QStringLiteral("lavfi")
         << QStringLiteral("-i")
         << QStringLiteral("color=black:s=1920x1080:d=%1:r=30").arg(duration, 0, 'f', 3)
         << encodeVideoArgs(outputPath);
    return runFfmpeg(args, errorOut);
}

bool exportSinglePiece(const TimelinePiece &piece, const QString &outputPath, QString *errorOut)
{
    if (piece.isGap)
        return exportBlackGap(piece.duration, outputPath, errorOut);

    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("error");
    args << inputArgsForSegment(piece.segment);
    args << encodeVideoArgs(outputPath);
    return runFfmpeg(args, errorOut);
}

bool concatFiles(const QStringList &segmentPaths, const QString &outputPath, QString *errorOut)
{
    if (segmentPaths.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No segments to export");
        return false;
    }

    if (segmentPaths.size() == 1)
        return QFile::copy(segmentPaths.front(), outputPath);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to create temporary directory");
        return false;
    }

    const QString listPath = tempDir.filePath(QStringLiteral("concat.txt"));
    QFile listFile(listPath);
    if (!listFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to write concat list");
        return false;
    }

    for (const QString &segmentPath : segmentPaths) {
        const QString escaped = segmentPath;
        listFile.write(QStringLiteral("file '%1'\n").arg(escaped).toUtf8());
    }
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

bool exportSegmentList(const QList<ExportSegment> &segments, const QString &outputPath, bool videoOnly,
                       QString *errorOut)
{
    if (segments.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No segments to export");
        return false;
    }

    const QList<TimelinePiece> pieces = buildTimelinePieces(segments);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to create temporary directory");
        return false;
    }

    QStringList segmentPaths;
    segmentPaths.reserve(pieces.size());

    for (int i = 0; i < pieces.size(); ++i) {
        const QString segmentPath = tempDir.filePath(QStringLiteral("piece_%1.mp4").arg(i));
        if (!exportSinglePiece(pieces.at(i), segmentPath, errorOut))
            return false;
        segmentPaths.append(segmentPath);
    }

    if (!videoOnly)
        return concatFiles(segmentPaths, outputPath, errorOut);

    return concatFiles(segmentPaths, outputPath, errorOut);
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

QString escapeDrawText(const QString &text)
{
    QString escaped;
    escaped.reserve(text.size() + 8);
    for (const QChar ch : text) {
        if (ch == QLatin1Char('\\') || ch == QLatin1Char('\'') || ch == QLatin1Char(':')
            || ch == QLatin1Char('%'))
            escaped += QLatin1Char('\\');
        escaped += ch;
    }
    return escaped;
}

bool burnTextOverlays(const QString &inputPath, const QList<ExportTextOverlay> &overlays,
                      const QString &outputPath, QString *errorOut)
{
    if (overlays.isEmpty()) {
        if (QFile::exists(outputPath))
            QFile::remove(outputPath);
        return QFile::copy(inputPath, outputPath);
    }

    QStringList filterParts;
    QString inLabel = QStringLiteral("[0:v]");
    QString outLabel = inLabel;

    for (int i = 0; i < overlays.size(); ++i) {
        const ExportTextOverlay &overlay = overlays.at(i);
        const QString nextLabel = QStringLiteral("[vt%1]").arg(i);
        const double end = overlay.timelineStart + overlay.duration;
        filterParts.append(QStringLiteral("%1drawtext=text='%2':fontsize=42:fontcolor=white:borderw=2:bordercolor=black@0.5:x=(w-text_w)/2:y=h*0.85:enable='between(t,%3,%4)'%5")
                               .arg(outLabel,
                                    escapeDrawText(overlay.text),
                                    QString::number(overlay.timelineStart, 'f', 3),
                                    QString::number(end, 'f', 3),
                                    nextLabel));
        outLabel = nextLabel;
    }

    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("error")
         << QStringLiteral("-i") << inputPath
         << QStringLiteral("-filter_complex") << filterParts.join(QLatin1Char(';'))
         << QStringLiteral("-map") << outLabel
         << QStringLiteral("-c:v") << QStringLiteral("libx264")
         << QStringLiteral("-preset") << QStringLiteral("fast")
         << QStringLiteral("-crf") << QStringLiteral("23")
         << QStringLiteral("-an")
         << QStringLiteral("-y")
         << outputPath;

    return runFfmpeg(args, errorOut);
}

} // namespace

bool TimelineExporter::exportTimeline(const QList<ExportSegment> &videoSegments,
                                      const QList<ExportSegment> &audioSegments,
                                      const QList<ExportTextOverlay> &textOverlays,
                                      const QString &outputPath, QString *errorOut)
{
    if (videoSegments.isEmpty() && audioSegments.isEmpty() && textOverlays.isEmpty()) {
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
    const bool hasAudio = !audioSegments.isEmpty();
    if (!exportSegmentList(videoSegments, videoPath, true, errorOut))
        return false;

    const QString videoWithTextPath = textOverlays.isEmpty()
                                          ? videoPath
                                          : tempDir.filePath(QStringLiteral("video_text.mp4"));
    if (!textOverlays.isEmpty()) {
        if (!burnTextOverlays(videoPath, textOverlays, videoWithTextPath, errorOut))
            return false;
    }

    if (!hasAudio) {
        if (QFile::exists(outputPath))
            QFile::remove(outputPath);
        return QFile::copy(videoWithTextPath, outputPath);
    }

    return muxAudioOverlay(videoWithTextPath, sortedSegments(audioSegments), outputPath, errorOut);
}
