#include "EditorState.h"

#include "AssetLibrary.h"
#include "engine/TimelineExporter.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QtMath>

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

QUrl EditorState::fileUrl(const QString &path) const
{
    if (path.isEmpty())
        return {};
    return QUrl::fromLocalFile(path);
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
    for (const Track &track : m_tracks) {
        if (track.type != QStringLiteral("video"))
            continue;

        for (const Clip &clip : track.clips) {
            if (m_playheadSeconds >= clip.start && m_playheadSeconds < clip.start + clip.duration)
                return clipToMap(clip);
        }
    }
    return {};
}

double EditorState::sourceTimeAtPlayhead() const
{
    const QVariantMap clip = activeVideoClipAtPlayhead();
    if (clip.isEmpty())
        return 0.0;

    const double start = clip.value(QStringLiteral("start")).toDouble();
    const double inPoint = clip.value(QStringLiteral("inPoint")).toDouble();
    return inPoint + (m_playheadSeconds - start);
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

    const QString kind = asset.value(QStringLiteral("kind")).toString();
    const QString trackType = m_tracks[trackIndex].type;

    if (trackType == QStringLiteral("audio") && kind != QStringLiteral("audio"))
        return;
    if (trackType == QStringLiteral("video") && kind == QStringLiteral("audio"))
        return;
    if (trackType == QStringLiteral("text"))
        return;

    const double duration = clipDurationForAsset(assetIndex);
    const double start = qMax(0.0, atSeconds);

    Track &track = m_tracks[trackIndex];
    track.clips.append({
        .name = asset.value(QStringLiteral("name")).toString(),
        .path = asset.value(QStringLiteral("path")).toString(),
        .kind = kind,
        .thumbnailPath = asset.value(QStringLiteral("thumbnailPath")).toString(),
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

    track.clips[clipIndex].start = qMax(0.0, newStart);
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
                .start = clipObject.value(QStringLiteral("start")).toDouble(),
                .duration = clipObject.value(QStringLiteral("duration")).toDouble(),
                .inPoint = clipObject.value(QStringLiteral("inPoint")).toDouble(),
                .outPoint = clipObject.value(QStringLiteral("outPoint")).toDouble(),
                .assetIndex = clipObject.value(QStringLiteral("assetIndex")).toInt(-1),
            });
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

    QList<ExportSegment> segments;
    for (const Track &track : m_tracks) {
        if (track.type != QStringLiteral("video"))
            continue;

        for (const Clip &clip : track.clips) {
            if (clip.kind != QStringLiteral("video") && clip.kind != QStringLiteral("image"))
                continue;

            segments.append({
                .path = clip.path,
                .kind = clip.kind,
                .timelineStart = clip.start,
                .inPoint = clip.inPoint,
                .duration = clip.duration,
            });
        }
    }

    QString error;
    const bool ok = TimelineExporter::exportVideo(segments, outputPath, &error);
    setLastMessage(ok ? QStringLiteral("Export complete") : error);
    emit exportFinished(ok);
}
