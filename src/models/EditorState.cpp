#include "EditorState.h"

#include "AssetLibrary.h"
#include "engine/TimelineExporter.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QtMath>
#include <algorithm>

EditorState::EditorState(AssetLibrary *assetLibrary, QObject *parent)
    : QObject(parent)
    , m_assetLibrary(assetLibrary)
{
    resetTimeline();

    m_playbackTimer.setInterval(static_cast<int>(kPlaybackTickSeconds * 1000.0));
    connect(&m_playbackTimer, &QTimer::timeout, this, [this] {
        const double next = m_playheadSeconds + kPlaybackTickSeconds;
        if (next >= durationSeconds()) {
            setPlayheadSeconds(durationSeconds());
            setPlaying(false);
            return;
        }
        setPlayheadSeconds(next);
    });
}

void EditorState::resetTimeline()
{
    m_tracks = {
        {QStringLiteral("video"), {}},
        {QStringLiteral("text"), {}},
        {QStringLiteral("audio"), {}},
    };
}

QVariantList EditorState::tracks() const
{
    QVariantList result;
    result.reserve(m_tracks.size());

    for (const Track &track : m_tracks) {
        QVariantList clips;
        clips.reserve(track.clips.size());

        for (const Clip &clip : track.clips)
            clips.append(clipToMap(clip));

        result.append(QVariantMap{
            {QStringLiteral("type"), track.type},
            {QStringLiteral("clips"), clips},
        });
    }

    return result;
}

QVariantMap EditorState::clipToMap(const Clip &clip) const
{
    return {
        {QStringLiteral("name"), clip.name},
        {QStringLiteral("path"), clip.path},
        {QStringLiteral("kind"), clip.kind},
        {QStringLiteral("thumbnailPath"), clip.thumbnailPath},
        {QStringLiteral("filmstripPath"), clip.filmstripPath},
        {QStringLiteral("start"), clip.start},
        {QStringLiteral("duration"), clip.duration},
        {QStringLiteral("inPoint"), clip.inPoint},
        {QStringLiteral("outPoint"), clip.outPoint},
        {QStringLiteral("assetIndex"), clip.assetIndex},
    };
}

double EditorState::durationSeconds() const
{
    double maxEnd = 0.0;
    for (const Track &track : m_tracks) {
        for (const Clip &clip : track.clips)
            maxEnd = qMax(maxEnd, clip.start + clip.duration);
    }
    return maxEnd;
}

QVariantMap EditorState::selectedClipData() const
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return {};
    return clipAt(m_selectedTrack, m_selectedClip);
}

void EditorState::setPlayheadSeconds(double seconds)
{
    const double clamped = qBound(0.0, seconds, qMax(durationSeconds(), 0.0));
    if (qFuzzyCompare(m_playheadSeconds, clamped))
        return;

    m_playheadSeconds = clamped;
    emit playheadSecondsChanged();
}

void EditorState::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;

    m_playing = playing;
    if (m_playing) {
        if (qFuzzyCompare(m_playheadSeconds, durationSeconds()) && durationSeconds() > 0.0)
            setPlayheadSeconds(0.0);
        m_playbackTimer.start();
    } else {
        m_playbackTimer.stop();
    }
    emit playingChanged();
}

void EditorState::setSnapEnabled(bool enabled)
{
    if (m_snapEnabled == enabled)
        return;

    m_snapEnabled = enabled;
    emit snapEnabledChanged();
}

QUrl EditorState::fileUrl(const QString &path) const
{
    if (path.isEmpty())
        return {};
    return QUrl::fromLocalFile(path);
}

QString EditorState::imageUrl(const QString &path) const
{
    if (path.isEmpty())
        return {};
    return QStringLiteral("image://drift/") + QString::fromUtf8(QUrl::toPercentEncoding(path));
}

void EditorState::setProjectName(const QString &name)
{
    if (m_projectName == name)
        return;

    m_projectName = name;
    emit projectNameChanged();
}

void EditorState::setLastMessage(const QString &message)
{
    if (m_lastMessage == message)
        return;

    m_lastMessage = message;
    emit lastMessageChanged();
}

bool EditorState::clipContainsTime(const Clip &clip, double seconds) const
{
    return seconds >= clip.start && seconds < clip.start + clip.duration;
}

double EditorState::snapTime(double seconds) const
{
    if (!m_snapEnabled)
        return qMax(0.0, seconds);

    QList<double> targets = {0.0, m_playheadSeconds};
    for (const Track &track : m_tracks) {
        for (const Clip &clip : track.clips) {
            targets.append(clip.start);
            targets.append(clip.start + clip.duration);
        }
    }

    double best = seconds;
    double bestDistance = kSnapThresholdSeconds;
    for (double target : targets) {
        const double distance = qAbs(target - seconds);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = target;
        }
    }

    return qMax(0.0, best);
}

double EditorState::resolveClipStart(const Track &track, int excludeClipIndex, double desiredStart,
                                     double duration) const
{
    double start = snapTime(desiredStart);

    struct Interval {
        double begin;
        double end;
    };
    QList<Interval> intervals;
    intervals.reserve(track.clips.size());

    for (int i = 0; i < track.clips.size(); ++i) {
        if (i == excludeClipIndex)
            continue;
        const Clip &clip = track.clips.at(i);
        intervals.append({clip.start, clip.start + clip.duration});
    }

    std::sort(intervals.begin(), intervals.end(),
              [](const Interval &a, const Interval &b) { return a.begin < b.begin; });

    bool adjusted = true;
    while (adjusted) {
        adjusted = false;
        for (const Interval &interval : intervals) {
            if (start < interval.end && start + duration > interval.begin) {
                start = interval.end;
                adjusted = true;
            }
        }
    }

    return qMax(0.0, start);
}

int EditorState::defaultTrackForKind(const QString &kind) const
{
    const QString trackType = kind == QStringLiteral("audio") ? QStringLiteral("audio")
                                                              : QStringLiteral("video");

    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].type == trackType)
            return i;
    }
    return -1;
}

double EditorState::clipDurationForAsset(int assetIndex) const
{
    const QVariantMap asset = m_assetLibrary->assetAt(assetIndex);
    const QString kind = asset.value(QStringLiteral("kind")).toString();
    const double sourceDuration = asset.value(QStringLiteral("durationSeconds")).toDouble();

    if (kind == QStringLiteral("image"))
        return kImageClipDurationSeconds;

    if (sourceDuration > 0.0)
        return sourceDuration;

    return kImageClipDurationSeconds;
}

double EditorState::sourceDurationForClip(const Clip &clip) const
{
    if (clip.assetIndex >= 0) {
        const double assetDuration = m_assetLibrary->assetAt(clip.assetIndex)
                                         .value(QStringLiteral("durationSeconds"))
                                         .toDouble();
        if (assetDuration > 0.0)
            return assetDuration;
    }

    if (clip.kind == QStringLiteral("image"))
        return kImageClipDurationSeconds;

    return qMax(clip.outPoint, clip.duration);
}

QString EditorState::thumbnailForAsset(int assetIndex) const
{
    return m_assetLibrary->thumbnailAt(assetIndex);
}

QVariantMap EditorState::clipAt(int trackIndex, int clipIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return {};
    if (clipIndex < 0 || clipIndex >= m_tracks[trackIndex].clips.size())
        return {};

    return clipToMap(m_tracks[trackIndex].clips.at(clipIndex));
}

QVariantMap EditorState::activeVideoClipAtPlayhead() const
{
    QVariantMap result;
    for (const Track &track : m_tracks) {
        if (track.type != QStringLiteral("video"))
            continue;

        for (const Clip &clip : track.clips) {
            if (clipContainsTime(clip, m_playheadSeconds))
                result = clipToMap(clip);
        }
    }
    return result;
}

QVariantMap EditorState::activeAudioClipAtPlayhead() const
{
    QVariantMap result;
    for (const Track &track : m_tracks) {
        if (track.type != QStringLiteral("audio"))
            continue;

        for (const Clip &clip : track.clips) {
            if (clipContainsTime(clip, m_playheadSeconds))
                result = clipToMap(clip);
        }
    }
    return result;
}

double EditorState::sourceTimeForClip(const QVariantMap &clip) const
{
    if (clip.isEmpty())
        return 0.0;

    const double start = clip.value(QStringLiteral("start")).toDouble();
    const double inPoint = clip.value(QStringLiteral("inPoint")).toDouble();
    return inPoint + (m_playheadSeconds - start);
}

double EditorState::sourceTimeAtPlayhead() const
{
    return sourceTimeForClip(activeVideoClipAtPlayhead());
}

void EditorState::addClipFromAsset(int assetIndex)
{
    const QVariantMap asset = m_assetLibrary->assetAt(assetIndex);
    if (asset.isEmpty())
        return;

    const QString kind = asset.value(QStringLiteral("kind")).toString();
    const int trackIndex = defaultTrackForKind(kind);
    if (trackIndex < 0)
        return;

    addClipFromAssetAt(assetIndex, trackIndex, m_playheadSeconds);
}

void EditorState::addClipFromAssetAt(int assetIndex, int trackIndex, double atSeconds)
{
    if (!m_assetLibrary || trackIndex < 0 || trackIndex >= m_tracks.size())
        return;

    const QVariantMap asset = m_assetLibrary->assetAt(assetIndex);
    if (asset.isEmpty())
        return;

    m_assetLibrary->ensureMedia(assetIndex);
    const QString thumbnailPath = m_assetLibrary->thumbnailAt(assetIndex);
    const QString filmstripPath = m_assetLibrary->filmstripAt(assetIndex);

    const QString kind = asset.value(QStringLiteral("kind")).toString();
    const QString trackType = m_tracks[trackIndex].type;

    if (trackType == QStringLiteral("audio") && kind != QStringLiteral("audio"))
        return;
    if (trackType == QStringLiteral("video") && kind == QStringLiteral("audio"))
        return;
    if (trackType == QStringLiteral("text"))
        return;

    const double duration = clipDurationForAsset(assetIndex);
    Track &track = m_tracks[trackIndex];
    const double start = resolveClipStart(track, -1, atSeconds, duration);

    track.clips.append({
        .name = asset.value(QStringLiteral("name")).toString(),
        .path = asset.value(QStringLiteral("path")).toString(),
        .kind = kind,
        .thumbnailPath = thumbnailPath,
        .filmstripPath = filmstripPath,
        .start = start,
        .duration = duration,
        .inPoint = 0.0,
        .outPoint = duration,
        .assetIndex = assetIndex,
    });

    emit tracksChanged();
    selectClip(trackIndex, track.clips.size() - 1);
}

void EditorState::selectClip(int trackIndex, int clipIndex)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;
    if (clipIndex < 0 || clipIndex >= m_tracks[trackIndex].clips.size())
        return;

    m_selectedTrack = trackIndex;
    m_selectedClip = clipIndex;
    emit selectionChanged();
}

void EditorState::clearSelection()
{
    if (m_selectedTrack < 0 && m_selectedClip < 0)
        return;

    m_selectedTrack = -1;
    m_selectedClip = -1;
    emit selectionChanged();
}

void EditorState::deleteSelectedClip()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;
    if (m_selectedTrack >= m_tracks.size())
        return;

    Track &track = m_tracks[m_selectedTrack];
    if (m_selectedClip < 0 || m_selectedClip >= track.clips.size())
        return;

    track.clips.removeAt(m_selectedClip);
    clearSelection();
    emit tracksChanged();
    setLastMessage(QStringLiteral("Clip deleted"));
}

void EditorState::moveClip(int trackIndex, int clipIndex, double newStart)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;

    Track &track = m_tracks[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    Clip &clip = track.clips[clipIndex];
    clip.start = resolveClipStart(track, clipIndex, newStart, clip.duration);
    emit tracksChanged();
}

void EditorState::splitAtPlayhead()
{
    bool splitAny = false;

    for (Track &track : m_tracks) {
        for (int clipIndex = 0; clipIndex < track.clips.size(); ++clipIndex) {
            Clip &clip = track.clips[clipIndex];
            if (!clipContainsTime(clip, m_playheadSeconds))
                continue;
            if (qFuzzyCompare(m_playheadSeconds, clip.start))
                continue;

            const double offset = m_playheadSeconds - clip.start;
            if (clip.duration - offset < kMinClipDurationSeconds)
                continue;
            if (offset < kMinClipDurationSeconds)
                continue;

            Clip tail = clip;
            tail.start = m_playheadSeconds;
            tail.inPoint = clip.inPoint + offset;
            tail.duration = clip.duration - offset;

            clip.duration = offset;
            clip.outPoint = clip.inPoint + offset;

            track.clips.insert(clipIndex + 1, tail);
            splitAny = true;
            ++clipIndex;
        }
    }

    if (splitAny) {
        emit tracksChanged();
        setLastMessage(QStringLiteral("Split at playhead"));
    } else {
        setLastMessage(QStringLiteral("Nothing to split at playhead"));
    }
}

void EditorState::trimClipLeft(int trackIndex, int clipIndex, double newStart)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;

    Track &track = m_tracks[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    Clip &clip = track.clips[clipIndex];
    const double snappedStart = snapTime(newStart);
    const double delta = snappedStart - clip.start;
    if (qFuzzyIsNull(delta))
        return;

    if (delta > 0.0) {
        if (clip.duration - delta < kMinClipDurationSeconds)
            return;
        if (clip.inPoint + delta > clip.outPoint - kMinClipDurationSeconds)
            return;

        clip.start += delta;
        clip.inPoint += delta;
        clip.duration -= delta;
    } else {
        const double extendBy = -delta;
        if (extendBy > clip.inPoint + 1e-6)
            return;

        clip.start = snappedStart;
        clip.inPoint -= extendBy;
        clip.duration += extendBy;
    }

    emit tracksChanged();
}

void EditorState::trimClipRight(int trackIndex, int clipIndex, double newEnd)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;

    Track &track = m_tracks[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    Clip &clip = track.clips[clipIndex];
    const double snappedEnd = snapTime(newEnd);
    double newDuration = snappedEnd - clip.start;

    const double maxDuration = sourceDurationForClip(clip) - clip.inPoint;
    newDuration = qBound(kMinClipDurationSeconds, newDuration, maxDuration);

    clip.duration = newDuration;
    clip.outPoint = clip.inPoint + newDuration;
    emit tracksChanged();
}

void EditorState::setClipTrim(int trackIndex, int clipIndex, double inPoint, double outPoint)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;

    Track &track = m_tracks[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    Clip &clip = track.clips[clipIndex];
    const double sourceDuration = sourceDurationForClip(clip);
    const double clampedIn = qBound(0.0, inPoint, sourceDuration - kMinClipDurationSeconds);
    const double clampedOut = qBound(clampedIn + kMinClipDurationSeconds, outPoint, sourceDuration);
    const double newDuration = clampedOut - clampedIn;

    clip.inPoint = clampedIn;
    clip.outPoint = clampedOut;
    clip.duration = newDuration;
    emit tracksChanged();
}

void EditorState::saveProject(const QUrl &url)
{
    const QString path = url.toLocalFile();
    if (path.isEmpty()) {
        setLastMessage(QStringLiteral("Invalid save path"));
        return;
    }

    QJsonArray tracksArray;
    for (const Track &track : m_tracks) {
        QJsonArray clipsArray;
        for (const Clip &clip : track.clips) {
            clipsArray.append(QJsonObject{
                {QStringLiteral("name"), clip.name},
                {QStringLiteral("path"), clip.path},
                {QStringLiteral("kind"), clip.kind},
                {QStringLiteral("thumbnailPath"), clip.thumbnailPath},
                {QStringLiteral("filmstripPath"), clip.filmstripPath},
                {QStringLiteral("start"), clip.start},
                {QStringLiteral("duration"), clip.duration},
                {QStringLiteral("inPoint"), clip.inPoint},
                {QStringLiteral("outPoint"), clip.outPoint},
                {QStringLiteral("assetIndex"), clip.assetIndex},
            });
        }

        tracksArray.append(QJsonObject{
            {QStringLiteral("type"), track.type},
            {QStringLiteral("clips"), clipsArray},
        });
    }

    const QJsonObject root{
        {QStringLiteral("version"), 1},
        {QStringLiteral("projectName"), m_projectName},
        {QStringLiteral("playheadSeconds"), m_playheadSeconds},
        {QStringLiteral("snapEnabled"), m_snapEnabled},
        {QStringLiteral("assets"), m_assetLibrary->toJsonArray()},
        {QStringLiteral("tracks"), tracksArray},
    };

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastMessage(QStringLiteral("Failed to save project"));
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    setLastMessage(QStringLiteral("Project saved"));
}

void EditorState::loadTracksFromJson(const QJsonArray &tracksArray)
{
    resetTimeline();

    for (int i = 0; i < tracksArray.size() && i < m_tracks.size(); ++i) {
        const QJsonObject trackObject = tracksArray.at(i).toObject();
        m_tracks[i].type = trackObject.value(QStringLiteral("type")).toString(m_tracks[i].type);

        const QJsonArray clipsArray = trackObject.value(QStringLiteral("clips")).toArray();
        for (const QJsonValue &clipValue : clipsArray) {
            const QJsonObject clipObject = clipValue.toObject();
            m_tracks[i].clips.append({
                .name = clipObject.value(QStringLiteral("name")).toString(),
                .path = clipObject.value(QStringLiteral("path")).toString(),
                .kind = clipObject.value(QStringLiteral("kind")).toString(),
                .thumbnailPath = clipObject.value(QStringLiteral("thumbnailPath")).toString(),
                .filmstripPath = clipObject.value(QStringLiteral("filmstripPath")).toString(),
                .start = clipObject.value(QStringLiteral("start")).toDouble(),
                .duration = clipObject.value(QStringLiteral("duration")).toDouble(),
                .inPoint = clipObject.value(QStringLiteral("inPoint")).toDouble(),
                .outPoint = clipObject.value(QStringLiteral("outPoint")).toDouble(),
                .assetIndex = clipObject.value(QStringLiteral("assetIndex")).toInt(-1),
            });
        }
    }

    if (m_assetLibrary) {
        for (Track &track : m_tracks) {
            for (Clip &clip : track.clips) {
                if (!clip.filmstripPath.isEmpty())
                    continue;
                if (clip.assetIndex >= 0) {
                    m_assetLibrary->ensureMedia(clip.assetIndex);
                    clip.filmstripPath = m_assetLibrary->filmstripAt(clip.assetIndex);
                }
                if (clip.filmstripPath.isEmpty())
                    clip.filmstripPath = clip.thumbnailPath;
            }
        }
    }
}

void EditorState::loadProject(const QUrl &url)
{
    const QString path = url.toLocalFile();
    if (path.isEmpty()) {
        setLastMessage(QStringLiteral("Invalid project path"));
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setLastMessage(QStringLiteral("Failed to open project"));
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        setLastMessage(QStringLiteral("Invalid project file"));
        return;
    }

    const QJsonObject root = document.object();
    setPlaying(false);
    setProjectName(root.value(QStringLiteral("projectName")).toString(QStringLiteral("Untitled Project")));
    setSnapEnabled(root.value(QStringLiteral("snapEnabled")).toBool(true));
    m_assetLibrary->loadFromJsonArray(root.value(QStringLiteral("assets")).toArray());
    loadTracksFromJson(root.value(QStringLiteral("tracks")).toArray());
    clearSelection();
    setPlayheadSeconds(root.value(QStringLiteral("playheadSeconds")).toDouble());
    emit tracksChanged();
    setLastMessage(QStringLiteral("Project loaded"));
}

void EditorState::exportProject(const QUrl &outputUrl)
{
    const QString outputPath = outputUrl.toLocalFile();
    if (outputPath.isEmpty()) {
        setLastMessage(QStringLiteral("Invalid export path"));
        emit exportFinished(false);
        return;
    }

    QList<ExportSegment> videoSegments;
    QList<ExportSegment> audioSegments;

    for (const Track &track : m_tracks) {
        if (track.type == QStringLiteral("video")) {
            for (const Clip &clip : track.clips) {
                if (clip.kind != QStringLiteral("video") && clip.kind != QStringLiteral("image"))
                    continue;

                videoSegments.append({
                    .path = clip.path,
                    .kind = clip.kind,
                    .timelineStart = clip.start,
                    .inPoint = clip.inPoint,
                    .duration = clip.duration,
                });
            }
        } else if (track.type == QStringLiteral("audio")) {
            for (const Clip &clip : track.clips) {
                audioSegments.append({
                    .path = clip.path,
                    .kind = QStringLiteral("audio"),
                    .timelineStart = clip.start,
                    .inPoint = clip.inPoint,
                    .duration = clip.duration,
                });
            }
        }
    }

    QString error;
    const bool ok = TimelineExporter::exportTimeline(videoSegments, audioSegments, outputPath, &error);
    setLastMessage(ok ? QStringLiteral("Export complete") : error);
    emit exportFinished(ok);
}
