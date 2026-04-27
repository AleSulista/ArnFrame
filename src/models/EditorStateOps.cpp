#include "EditorState.h"

#include "AssetLibrary.h"
#include "engine/MediaThumbnail.h"
#include "engine/MediaWaveform.h"
#include "engine/TimelineExporter.h"

#include <QFile>
#include <QtConcurrent>

void EditorState::setRippleEnabled(bool enabled)
{
    if (m_rippleEnabled == enabled)
        return;
    m_rippleEnabled = enabled;
    emit rippleEnabledChanged();
}

QVariantList EditorState::bookmarks() const
{
    QVariantList result;
    for (const Bookmark &bookmark : m_bookmarks) {
        result.append(QVariantMap{
            {QStringLiteral("seconds"), bookmark.seconds},
            {QStringLiteral("label"), bookmark.label},
        });
    }
    return result;
}

EditorState::Snapshot EditorState::captureSnapshot() const
{
    return {
        .tracks = m_tracks,
        .bookmarks = m_bookmarks,
        .playheadSeconds = m_playheadSeconds,
        .selectedTrack = m_selectedTrack,
        .selectedClip = m_selectedClip,
    };
}

void EditorState::restoreSnapshot(const Snapshot &snapshot)
{
    m_tracks = snapshot.tracks;
    m_bookmarks = snapshot.bookmarks;
    m_selectedTrack = snapshot.selectedTrack;
    m_selectedClip = snapshot.selectedClip;
    setPlayheadSeconds(snapshot.playheadSeconds);
    emit tracksChanged();
    emit bookmarksChanged();
    emit selectionChanged();
}

void EditorState::pushUndo()
{
    m_undoStack.append(captureSnapshot());
    if (m_undoStack.size() > kMaxUndoSteps)
        m_undoStack.removeFirst();
    clearRedo();
    emit undoStackChanged();
}

void EditorState::clearRedo()
{
    if (m_redoStack.isEmpty())
        return;
    m_redoStack.clear();
    emit undoStackChanged();
}

void EditorState::finishEdit(const QString &message)
{
    emit tracksChanged();
    setLastMessage(message);
}

bool EditorState::trackAllowsKind(const Track &track, const QString &kind) const
{
    if (track.type == QStringLiteral("audio"))
        return kind == QStringLiteral("audio");
    if (track.type == QStringLiteral("text"))
        return kind == QStringLiteral("text");
    if (track.type == QStringLiteral("video"))
        return kind == QStringLiteral("video") || kind == QStringLiteral("image");
    return false;
}

void EditorState::applyRippleShift(Track &track, int fromClipIndex, double delta)
{
    if (!m_rippleEnabled || qFuzzyIsNull(delta))
        return;

    for (int i = fromClipIndex + 1; i < track.clips.size(); ++i)
        track.clips[i].start = qMax(0.0, track.clips[i].start + delta);
}

void EditorState::undo()
{
    if (m_undoStack.isEmpty())
        return;

    m_redoStack.append(captureSnapshot());
    const Snapshot snapshot = m_undoStack.takeLast();
    restoreSnapshot(snapshot);
    emit undoStackChanged();
    setLastMessage(QStringLiteral("Undo"));
}

void EditorState::redo()
{
    if (m_redoStack.isEmpty())
        return;

    m_undoStack.append(captureSnapshot());
    const Snapshot snapshot = m_redoStack.takeLast();
    restoreSnapshot(snapshot);
    emit undoStackChanged();
    setLastMessage(QStringLiteral("Redo"));
}

void EditorState::duplicateSelectedClip()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;

    Track &track = m_tracks[m_selectedTrack];
    if (m_selectedClip >= track.clips.size())
        return;

    pushUndo();
    const Clip original = track.clips.at(m_selectedClip);
    Clip copy = original;
    copy.start = resolveClipStart(track, -1, original.start + original.duration, original.duration);

    track.clips.append(copy);
    finishEdit(QStringLiteral("Clip duplicated"));
    selectClip(m_selectedTrack, track.clips.size() - 1);
}

void EditorState::alignSelectedClipLeft()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;

    Track &track = m_tracks[m_selectedTrack];
    if (m_selectedClip >= track.clips.size())
        return;

    pushUndo();
    Clip &clip = track.clips[m_selectedClip];
    double target = 0.0;
    for (int i = 0; i < track.clips.size(); ++i) {
        if (i == m_selectedClip)
            continue;
        const Clip &other = track.clips.at(i);
        if (other.start + other.duration <= clip.start)
            target = qMax(target, other.start + other.duration);
    }
    clip.start = resolveClipStart(track, m_selectedClip, target, clip.duration);
    finishEdit(QStringLiteral("Aligned left"));
}

void EditorState::alignSelectedClipRight()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;

    Track &track = m_tracks[m_selectedTrack];
    if (m_selectedClip >= track.clips.size())
        return;

    pushUndo();
    Clip &clip = track.clips[m_selectedClip];
    double nextStart = durationSeconds();
    for (int i = 0; i < track.clips.size(); ++i) {
        if (i == m_selectedClip)
            continue;
        const Clip &other = track.clips.at(i);
        if (other.start >= clip.start + clip.duration)
            nextStart = qMin(nextStart, other.start);
    }
    const double newStart = qMax(0.0, nextStart - clip.duration);
    clip.start = resolveClipStart(track, m_selectedClip, newStart, clip.duration);
    finishEdit(QStringLiteral("Aligned right"));
}

void EditorState::moveClipToTrack(int trackIndex, int clipIndex, int newTrackIndex, double newStart)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;
    if (newTrackIndex < 0 || newTrackIndex >= m_tracks.size())
        return;

    Track &fromTrack = m_tracks[trackIndex];
    if (clipIndex < 0 || clipIndex >= fromTrack.clips.size())
        return;

    Track &toTrack = m_tracks[newTrackIndex];
    Clip clip = fromTrack.clips.at(clipIndex);
    if (!trackAllowsKind(toTrack, clip.kind))
        return;

    pushUndo();
    fromTrack.clips.removeAt(clipIndex);
    clip.start = resolveClipStart(toTrack, -1, newStart, clip.duration);
    toTrack.clips.append(clip);

    finishEdit(QStringLiteral("Clip moved"));
    selectClip(newTrackIndex, toTrack.clips.size() - 1);
}

void EditorState::addTextClip(const QString &text, double atSeconds)
{
    const int trackIndex = defaultTrackForKind(QStringLiteral("text"));
    if (trackIndex < 0)
        return;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;

    pushUndo();
    Track &track = m_tracks[trackIndex];
    const double startSeconds = atSeconds < 0.0 ? m_playheadSeconds : atSeconds;
    const double start = resolveClipStart(track, -1, startSeconds, kTextClipDurationSeconds);

    track.clips.append({
        .name = trimmed.left(32),
        .path = {},
        .kind = QStringLiteral("text"),
        .textContent = trimmed,
        .start = start,
        .duration = kTextClipDurationSeconds,
        .inPoint = 0.0,
        .outPoint = kTextClipDurationSeconds,
        .assetIndex = -1,
    });

    finishEdit(QStringLiteral("Text clip added"));
    selectClip(trackIndex, track.clips.size() - 1);
}

void EditorState::setClipStart(int trackIndex, int clipIndex, double start)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;

    Track &track = m_tracks[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    pushUndo();
    Clip &clip = track.clips[clipIndex];
    const double oldStart = clip.start;
    clip.start = resolveClipStart(track, clipIndex, start, clip.duration);
    applyRippleShift(track, clipIndex, clip.start - oldStart);
    finishEdit(QStringLiteral("Start updated"));
}

void EditorState::setClipDuration(int trackIndex, int clipIndex, double duration)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;

    Track &track = m_tracks[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    pushUndo();
    Clip &clip = track.clips[clipIndex];
    const double maxDuration = clip.kind == QStringLiteral("text")
                                   ? 300.0
                                   : sourceDurationForClip(clip) - clip.inPoint;
    clip.duration = qBound(kMinClipDurationSeconds, duration, maxDuration);
    clip.outPoint = clip.inPoint + clip.duration;
    finishEdit(QStringLiteral("Duration updated"));
}

void EditorState::setClipTextContent(int trackIndex, int clipIndex, const QString &text)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;

    Track &track = m_tracks[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    Clip &clip = track.clips[clipIndex];
    if (clip.kind != QStringLiteral("text"))
        return;

    pushUndo();
    clip.textContent = text.trimmed();
    clip.name = clip.textContent.left(32);
    finishEdit(QStringLiteral("Text updated"));
}

void EditorState::setTrackMuted(int trackIndex, bool muted)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;
    if (m_tracks[trackIndex].muted == muted)
        return;

    m_tracks[trackIndex].muted = muted;
    emit tracksChanged();
}

void EditorState::setTrackHidden(int trackIndex, bool hidden)
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return;
    if (m_tracks[trackIndex].hidden == hidden)
        return;

    m_tracks[trackIndex].hidden = hidden;
    emit tracksChanged();
}

bool EditorState::trackMuted(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return false;
    return m_tracks.at(trackIndex).muted;
}

bool EditorState::trackHidden(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size())
        return false;
    return m_tracks.at(trackIndex).hidden;
}

void EditorState::addBookmark(double seconds, const QString &label)
{
    m_bookmarks.append({
        .seconds = qMax(0.0, seconds),
        .label = label.isEmpty() ? QStringLiteral("Bookmark") : label,
    });
    emit bookmarksChanged();
    setLastMessage(QStringLiteral("Bookmark added"));
}

void EditorState::removeBookmark(int index)
{
    if (index < 0 || index >= m_bookmarks.size())
        return;
    m_bookmarks.removeAt(index);
    emit bookmarksChanged();
}

void EditorState::goToBookmark(int index)
{
    if (index < 0 || index >= m_bookmarks.size())
        return;
    setPlayheadSeconds(m_bookmarks.at(index).seconds);
}

void EditorState::freezeFrameAtPlayhead()
{
    const QVariantMap clip = activeVideoClipAtPlayhead();
    if (clip.isEmpty() || clip.value(QStringLiteral("kind")).toString() != QStringLiteral("video"))
    {
        setLastMessage(QStringLiteral("No video clip at playhead"));
        return;
    }

    const QString path = clip.value(QStringLiteral("path")).toString();
    const double sourceTime = sourceTimeForClip(clip);
    const QString thumb = MediaThumbnail::generateAtTime(path, sourceTime);
    if (thumb.isEmpty()) {
        setLastMessage(QStringLiteral("Failed to capture frame"));
        return;
    }

    pushUndo();
    const int trackIndex = defaultTrackForKind(QStringLiteral("image"));
    if (trackIndex < 0)
        return;

    Track &track = m_tracks[trackIndex];
    const double start = resolveClipStart(track, -1, m_playheadSeconds, kImageClipDurationSeconds);
    track.clips.append({
        .name = QStringLiteral("Freeze frame"),
        .path = path,
        .kind = QStringLiteral("image"),
        .thumbnailPath = thumb,
        .filmstripPath = thumb,
        .start = start,
        .duration = kImageClipDurationSeconds,
        .inPoint = sourceTime,
        .outPoint = sourceTime + kImageClipDurationSeconds,
        .assetIndex = -1,
    });

    finishEdit(QStringLiteral("Freeze frame added"));
}

QVariantList EditorState::waveformPeaks(const QString &path) const
{
    return MediaWaveform::peaks(path);
}
