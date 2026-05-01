#include "AppController.h"

#include "AssetLibrary.h"
#include "core/Clip.h"
#include "core/TimelineOps.h"
#include "core/commands/ProjectCommands.h"
#include "engine/EffectCatalog.h"
#include "engine/MediaThumbnail.h"
#include "engine/MediaWaveform.h"
#include "engine/TimelineExporter.h"

#include <QColor>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>
#include <QtMath>
#include <algorithm>

AppController::AppController(AssetLibrary *assetLibrary, QObject *parent)
    : QObject(parent)
    , m_assetLibrary(assetLibrary)
{
    m_project.resetToDefaultTimeline();
    if (m_assetLibrary)
        m_assetLibrary->setProject(&m_project);

    m_timelineModel.setProject(&m_project);
    m_clipListModel.setProject(&m_project);

    m_undoStack.setUndoLimit(kMaxUndoSteps);
    connect(&m_undoStack, &QUndoStack::indexChanged, this, &AppController::undoStackChanged);
    connect(&m_undoStack, &QUndoStack::indexChanged, this, [this] {
        m_timelineModel.refresh();
        m_clipListModel.refresh();
        emit tracksChanged();
        emit bookmarksChanged();
        emit projectNameChanged();
        emit selectionChanged();
    });

    m_playback.setProject(&m_project);
    connect(&m_playback, &PlaybackEngine::playheadUsChanged, this, [this](quint64 us) {
        if (!m_playing)
            return;
        const drift::TimeUs newUs = static_cast<drift::TimeUs>(us);
        if (m_playheadUs == newUs)
            return;
        m_playheadUs = newUs;
        emit playheadSecondsChanged();
    });
    connect(&m_playback, &PlaybackEngine::playingChanged, this, [this] {
        if (!m_playback.isPlaying() && m_playing) {
            m_playing = false;
            emit playingChanged();
        }
    });
    connect(this, &AppController::tracksChanged, this, [this] {
        m_timelineModel.refresh();
        m_clipListModel.refresh();
        m_playback.setProject(&m_project);
    });
    connect(this, &AppController::selectionChanged, this, [this] {
        m_clipListModel.setTrackIndex(m_selectedTrack >= 0 ? m_selectedTrack : 0);
    });
}

QVariantList AppController::tracks() const
{
    QVariantList result;
    result.reserve(m_project.tracks().size());

    for (const drift::Track &track : m_project.tracks()) {
        QVariantList clips;
        clips.reserve(track.clips.size());

        for (const drift::Clip &clip : track.clips)
            clips.append(clipToMap(clip));

        result.append(QVariantMap{
            {QStringLiteral("type"), drift::trackTypeToString(track.type)},
            {QStringLiteral("clips"), clips},
            {QStringLiteral("muted"), track.muted},
            {QStringLiteral("hidden"), track.hidden},
        });
    }

    return result;
}

namespace {

QVariantMap textStyleToMap(const drift::TextStyle &s)
{
    return {
        {QStringLiteral("fontFamily"), s.fontFamily},
        {QStringLiteral("pixelSize"), s.pixelSize},
        {QStringLiteral("color"), s.color.name(QColor::HexArgb)},
        {QStringLiteral("bold"), s.bold},
        {QStringLiteral("italic"), s.italic},
        {QStringLiteral("align"), drift::textAlignToString(s.align)},
        {QStringLiteral("outlineWidth"), s.outlineWidth},
        {QStringLiteral("outlineColor"), s.outlineColor.name(QColor::HexArgb)},
        {QStringLiteral("boxEnabled"), s.boxEnabled},
        {QStringLiteral("boxColor"), s.boxColor.name(QColor::HexArgb)},
        {QStringLiteral("boxPadding"), s.boxPadding},
    };
}

drift::KeyframeTrack<double> *trackForProp(drift::Clip &clip, const QString &prop)
{
    if (prop == QStringLiteral("opacity"))
        return &clip.opacity;
    if (prop == QStringLiteral("posX"))
        return &clip.posX;
    if (prop == QStringLiteral("posY"))
        return &clip.posY;
    if (prop == QStringLiteral("scale"))
        return &clip.scale;
    if (prop == QStringLiteral("rotation"))
        return &clip.rotation;
    if (prop == QStringLiteral("volume"))
        return &clip.volume;
    return nullptr;
}

const drift::KeyframeTrack<double> *trackForProp(const drift::Clip &clip, const QString &prop)
{
    return trackForProp(const_cast<drift::Clip &>(clip), prop);
}

drift::TextStyle textStyleForPreset(const QString &presetId)
{
    drift::TextStyle s;
    if (presetId == QStringLiteral("title")) {
        s.pixelSize = 96;
        s.bold = true;
        s.align = drift::TextAlign::Center;
        s.outlineWidth = 2.0;
        s.outlineColor = Qt::black;
    } else if (presetId == QStringLiteral("lower third")) {
        s.pixelSize = 48;
        s.bold = true;
        s.align = drift::TextAlign::Left;
        s.boxEnabled = true;
        s.boxColor = QColor(0, 0, 0, 160);
        s.boxPadding = 12.0;
    } else if (presetId == QStringLiteral("subtitle")) {
        s.pixelSize = 40;
        s.bold = false;
        s.align = drift::TextAlign::Center;
        s.boxEnabled = true;
        s.boxColor = QColor(0, 0, 0, 140);
        s.boxPadding = 8.0;
    }
    return s;
}

QVariantMap effectToMap(const drift::Effect &effect)
{
    const EffectDef *def = effectDefForId(effect.catalogId);
    QVariantList params;
    if (def) {
        for (const EffectParamDef &paramDef : def->params) {
            params.append(QVariantMap{
                {QStringLiteral("key"), paramDef.key},
                {QStringLiteral("label"), paramDef.label},
                {QStringLiteral("min"), paramDef.min},
                {QStringLiteral("max"), paramDef.max},
                {QStringLiteral("value"), effect.parameters.value(paramDef.key, paramDef.def)},
            });
        }
    }
    return {
        {QStringLiteral("catalogId"), effect.catalogId},
        {QStringLiteral("label"), def ? def->label : effect.name},
        {QStringLiteral("params"), params},
    };
}

QVariantList keyframeListToVariant(const drift::KeyframeTrack<double> &track, drift::TimeUs timelineStart)
{
    QVariantList out;
    for (auto it = track.keyframes().constBegin(); it != track.keyframes().constEnd(); ++it) {
        out.append(QVariantMap{
            {QStringLiteral("seconds"), drift::usToSeconds(timelineStart + it.key())},
            {QStringLiteral("value"), it.value()},
        });
    }
    return out;
}

QVariantMap keyframesToMap(const drift::Clip &clip)
{
    return {
        {QStringLiteral("opacity"), keyframeListToVariant(clip.opacity, clip.timelineStart)},
        {QStringLiteral("posX"), keyframeListToVariant(clip.posX, clip.timelineStart)},
        {QStringLiteral("posY"), keyframeListToVariant(clip.posY, clip.timelineStart)},
        {QStringLiteral("scale"), keyframeListToVariant(clip.scale, clip.timelineStart)},
        {QStringLiteral("rotation"), keyframeListToVariant(clip.rotation, clip.timelineStart)},
        {QStringLiteral("volume"), keyframeListToVariant(clip.volume, clip.timelineStart)},
    };
}

} // namespace

QVariantMap AppController::clipToMap(const drift::Clip &clip) const
{
    QVariantList effects;
    for (const drift::Effect &effect : clip.effects)
        effects.append(effectToMap(effect));

    return {
        {QStringLiteral("id"), clip.id},
        {QStringLiteral("name"), clip.name},
        {QStringLiteral("path"), clip.path},
        {QStringLiteral("kind"), drift::clipTypeToString(clip.type)},
        {QStringLiteral("thumbnailPath"), clip.thumbnailPath},
        {QStringLiteral("filmstripPath"), clip.filmstripPath},
        {QStringLiteral("textContent"), clip.textContent},
        {QStringLiteral("textStyle"), textStyleToMap(clip.textStyle)},
        {QStringLiteral("blendMode"), drift::blendModeToString(clip.blendMode)},
        {QStringLiteral("start"), drift::usToSeconds(clip.timelineStart)},
        {QStringLiteral("duration"), drift::usToSeconds(clip.timelineDuration)},
        {QStringLiteral("inPoint"), drift::usToSeconds(clip.srcIn)},
        {QStringLiteral("outPoint"), drift::usToSeconds(clip.srcOut)},
        {QStringLiteral("assetId"), clip.assetId},
        {QStringLiteral("assetIndex"), assetIndexForClip(clip)},
        {QStringLiteral("volume"), clip.volume.isEmpty() ? 1.0 : clip.volume.evaluateAt(0)},
        {QStringLiteral("effects"), effects},
        {QStringLiteral("keyframes"), keyframesToMap(clip)},
    };
}

int AppController::assetIndexForClip(const drift::Clip &clip) const
{
    if (clip.assetId.isEmpty())
        return -1;
    return m_project.assetIndex(clip.assetId);
}

double AppController::playheadSeconds() const
{
    return drift::usToSeconds(m_playheadUs);
}

double AppController::durationSeconds() const
{
    return drift::usToSeconds(m_project.durationUs());
}

QString AppController::projectName() const
{
    return m_project.name();
}

QVariantMap AppController::selectedClipData() const
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return {};
    return clipAt(m_selectedTrack, m_selectedClip);
}

void AppController::setPlayheadUs(drift::TimeUs us)
{
    const drift::TimeUs clamped = qBound<drift::TimeUs>(0, us, qMax(m_project.durationUs(), drift::TimeUs{0}));
    if (m_playheadUs == clamped)
        return;

    m_playheadUs = clamped;
    m_playback.setPlayheadUs(clamped);
    emit playheadSecondsChanged();
}

void AppController::setPlayheadSeconds(double seconds)
{
    setPlayheadUs(drift::secondsToUs(seconds));
}

void AppController::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;

    m_playing = playing;
    if (m_playing) {
        if (m_playheadUs >= m_project.durationUs() && m_project.durationUs() > 0)
            setPlayheadUs(0);
        m_playback.setPlayheadUs(m_playheadUs);
        m_playback.play();
    } else {
        m_playback.pause();
    }
    emit playingChanged();
}

void AppController::setSnapEnabled(bool enabled)
{
    if (m_snapEnabled == enabled)
        return;

    m_snapEnabled = enabled;
    emit snapEnabledChanged();
}

void AppController::setRippleEnabled(bool enabled)
{
    if (m_rippleEnabled == enabled)
        return;
    m_rippleEnabled = enabled;
    emit rippleEnabledChanged();
}

void AppController::setDraggingAssetIndex(int index)
{
    if (m_draggingAssetIndex == index)
        return;
    m_draggingAssetIndex = index;
    emit draggingAssetIndexChanged();
}

void AppController::setProjectName(const QString &name)
{
    if (m_project.name() == name)
        return;

    m_project.setName(name);
    emit projectNameChanged();
}

void AppController::setLastMessage(const QString &message)
{
    if (m_lastMessage == message)
        return;

    m_lastMessage = message;
    emit lastMessageChanged();
}

QUrl AppController::fileUrl(const QString &path) const
{
    if (path.isEmpty())
        return {};
    return QUrl::fromLocalFile(path);
}

QString AppController::imageUrl(const QString &path) const
{
    if (path.isEmpty())
        return {};
    return QStringLiteral("image://drift/") + QString::fromUtf8(QUrl::toPercentEncoding(path));
}

double AppController::snapTime(double seconds) const
{
    return drift::usToSeconds(
        drift::snapTime(m_project, drift::secondsToUs(seconds), m_snapEnabled, m_playheadUs));
}

drift::TimeUs AppController::clipDurationForAssetIndex(int assetIndex) const
{
    if (!m_assetLibrary)
        return drift::kImageClipDurationUs;
    return drift::clipDurationForAsset(m_project.asset(m_assetLibrary->assetIdAt(assetIndex)));
}

drift::TimeUs AppController::sourceDurationForClip(const drift::Clip &clip) const
{
    return drift::sourceDurationForClip(m_project, clip);
}

QVariantMap AppController::clipAt(int trackIndex, int clipIndex) const
{
    const QList<drift::Track> &tracks = m_project.tracks();
    if (trackIndex < 0 || trackIndex >= tracks.size())
        return {};
    if (clipIndex < 0 || clipIndex >= tracks[trackIndex].clips.size())
        return {};

    return clipToMap(tracks[trackIndex].clips.at(clipIndex));
}

QVariantMap AppController::activeVideoClipAtPlayhead() const
{
    QVariantMap result;
    for (const drift::Track &track : m_project.tracks()) {
        if (track.type != drift::TrackType::Video || track.hidden)
            continue;

        for (const drift::Clip &clip : track.clips) {
            if (clip.containsTime(m_playheadUs))
                result = clipToMap(clip);
        }
    }
    return result;
}

QVariantMap AppController::activeAudioClipAtPlayhead() const
{
    QVariantMap result;
    for (const drift::Track &track : m_project.tracks()) {
        if (track.type != drift::TrackType::Audio || track.muted || track.hidden)
            continue;

        for (const drift::Clip &clip : track.clips) {
            if (clip.containsTime(m_playheadUs))
                result = clipToMap(clip);
        }
    }
    return result;
}

double AppController::sourceTimeForClip(const QVariantMap &clip) const
{
    if (clip.isEmpty())
        return 0.0;

    const double start = clip.value(QStringLiteral("start")).toDouble();
    const double inPoint = clip.value(QStringLiteral("inPoint")).toDouble();
    return inPoint + (playheadSeconds() - start);
}

double AppController::sourceTimeAtPlayhead() const
{
    return sourceTimeForClip(activeVideoClipAtPlayhead());
}

void AppController::pushProjectEdit(const drift::Project &before, const QString &text)
{
    m_undoStack.push(new drift::ProjectSnapshotCommand(&m_project, before, m_project, text));
}

void AppController::finishEdit(const QString &message)
{
    m_playback.setPlayheadUs(m_playheadUs);
    emit tracksChanged();
    setLastMessage(message);
}

void AppController::applyRippleShift(drift::Track &track, int fromClipIndex, drift::TimeUs delta)
{
    if (!m_rippleEnabled || delta == 0)
        return;

    for (int i = fromClipIndex + 1; i < track.clips.size(); ++i)
        track.clips[i].timelineStart = qMax<drift::TimeUs>(0, track.clips[i].timelineStart + delta);
}

void AppController::addClipFromAsset(int assetIndex)
{
    const QVariantMap asset = m_assetLibrary ? m_assetLibrary->assetAt(assetIndex) : QVariantMap{};
    if (asset.isEmpty())
        return;

    const QString kind = asset.value(QStringLiteral("kind")).toString();
    const drift::ClipType clipType = drift::clipTypeFromString(kind);
    const int trackIndex = drift::defaultTrackForClipType(m_project, clipType);
    if (trackIndex < 0)
        return;

    addClipFromAssetAt(assetIndex, trackIndex, playheadSeconds());
}

void AppController::addClipFromAssetAt(int assetIndex, int trackIndex, double atSeconds)
{
    if (!m_assetLibrary || trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    const QVariantMap asset = m_assetLibrary->assetAt(assetIndex);
    if (asset.isEmpty())
        return;

    m_assetLibrary->ensureMedia(assetIndex);
    const QString thumbnailPath = m_assetLibrary->thumbnailAt(assetIndex);
    const QString filmstripPath = m_assetLibrary->filmstripAt(assetIndex);
    const QString kind = asset.value(QStringLiteral("kind")).toString();
    const drift::ClipType clipType = drift::clipTypeFromString(kind);

    drift::Track &track = m_project.tracks()[trackIndex];
    if (!track.allowsClipType(clipType))
        return;

    const drift::Project before = m_project;
    const drift::TimeUs duration = clipDurationForAssetIndex(assetIndex);
    const drift::TimeUs start = drift::resolveClipStart(m_project, track, -1, drift::secondsToUs(atSeconds),
                                                        duration, m_snapEnabled, m_playheadUs);

    drift::Clip clip;
    clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clip.assetId = m_assetLibrary->assetIdAt(assetIndex);
    clip.type = clipType;
    clip.name = asset.value(QStringLiteral("name")).toString();
    clip.path = asset.value(QStringLiteral("path")).toString();
    clip.thumbnailPath = thumbnailPath;
    clip.filmstripPath = filmstripPath;
    clip.timelineStart = start;
    clip.timelineDuration = duration;
    clip.srcIn = 0;
    clip.srcOut = duration;

    track.clips.append(clip);
    pushProjectEdit(before, QStringLiteral("Clip added"));
    finishEdit(QStringLiteral("Clip added"));
    selectClip(trackIndex, track.clips.size() - 1);
}

void AppController::selectClip(int trackIndex, int clipIndex)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;
    if (clipIndex < 0 || clipIndex >= m_project.tracks()[trackIndex].clips.size())
        return;

    m_selectedTrack = trackIndex;
    m_selectedClip = clipIndex;
    emit selectionChanged();
}

void AppController::clearSelection()
{
    if (m_selectedTrack < 0 && m_selectedClip < 0)
        return;

    m_selectedTrack = -1;
    m_selectedClip = -1;
    emit selectionChanged();
}

void AppController::deleteSelectedClip()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;
    if (m_selectedTrack >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[m_selectedTrack];
    if (m_selectedClip < 0 || m_selectedClip >= track.clips.size())
        return;

    const drift::Project before = m_project;
    track.clips.removeAt(m_selectedClip);
    pushProjectEdit(before, QStringLiteral("Clip deleted"));
    clearSelection();
    finishEdit(QStringLiteral("Clip deleted"));
}

void AppController::moveClip(int trackIndex, int clipIndex, double newStart)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    const drift::Project before = m_project;
    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs oldStart = clip.timelineStart;
    clip.timelineStart = drift::resolveClipStart(m_project, track, clipIndex, drift::secondsToUs(newStart),
                                                 clip.timelineDuration, m_snapEnabled, m_playheadUs);
    applyRippleShift(track, clipIndex, clip.timelineStart - oldStart);
    pushProjectEdit(before, QStringLiteral("Clip moved"));
    finishEdit(QStringLiteral("Clip moved"));
}

void AppController::splitAtPlayhead()
{
    const drift::Project before = m_project;
    bool splitAny = false;

    for (drift::Track &track : m_project.tracks()) {
        for (int clipIndex = 0; clipIndex < track.clips.size(); ++clipIndex) {
            drift::Clip &clip = track.clips[clipIndex];
            if (!clip.containsTime(m_playheadUs))
                continue;
            if (m_playheadUs == clip.timelineStart)
                continue;

            const drift::TimeUs offset = m_playheadUs - clip.timelineStart;
            if (clip.timelineDuration - offset < drift::kMinClipDurationUs)
                continue;
            if (offset < drift::kMinClipDurationUs)
                continue;

            drift::Clip tail = clip;
            tail.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            tail.timelineStart = m_playheadUs;
            tail.srcIn = clip.srcIn + offset;
            tail.timelineDuration = clip.timelineDuration - offset;

            clip.timelineDuration = offset;
            clip.srcOut = clip.srcIn + offset;

            track.clips.insert(clipIndex + 1, tail);
            splitAny = true;
            ++clipIndex;
        }
    }

    if (splitAny) {
        pushProjectEdit(before, QStringLiteral("Split at playhead"));
        finishEdit(QStringLiteral("Split at playhead"));
    } else {
        setLastMessage(QStringLiteral("Nothing to split at playhead"));
    }
}

void AppController::trimClipLeft(int trackIndex, int clipIndex, double newStart)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs snappedStart = drift::snapTime(m_project, drift::secondsToUs(newStart), m_snapEnabled,
                                                       m_playheadUs);
    const drift::TimeUs delta = snappedStart - clip.timelineStart;
    if (delta == 0)
        return;

    if (delta > 0) {
        if (clip.timelineDuration - delta < drift::kMinClipDurationUs)
            return;
        if (clip.srcIn + delta > clip.srcOut - drift::kMinClipDurationUs)
            return;

        clip.timelineStart += delta;
        clip.srcIn += delta;
        clip.timelineDuration -= delta;
    } else {
        const drift::TimeUs extendBy = -delta;
        if (extendBy > clip.srcIn)
            return;

        clip.timelineStart = snappedStart;
        clip.srcIn -= extendBy;
        clip.timelineDuration += extendBy;
    }

    emit tracksChanged();
}

void AppController::trimClipRight(int trackIndex, int clipIndex, double newEnd)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs snappedEnd = drift::snapTime(m_project, drift::secondsToUs(newEnd), m_snapEnabled,
                                                     m_playheadUs);
    drift::TimeUs newDuration = snappedEnd - clip.timelineStart;

    const drift::TimeUs maxDuration = sourceDurationForClip(clip) - clip.srcIn;
    newDuration = qBound(drift::kMinClipDurationUs, newDuration, maxDuration);

    clip.timelineDuration = newDuration;
    clip.srcOut = clip.srcIn + newDuration;
    emit tracksChanged();
}

void AppController::setClipTrim(int trackIndex, int clipIndex, double inPoint, double outPoint)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs sourceDuration = sourceDurationForClip(clip);
    const drift::TimeUs clampedIn = qBound<drift::TimeUs>(0, drift::secondsToUs(inPoint),
                                                          sourceDuration - drift::kMinClipDurationUs);
    const drift::TimeUs clampedOut = qBound(clampedIn + drift::kMinClipDurationUs, drift::secondsToUs(outPoint),
                                            sourceDuration);
    const drift::TimeUs newDuration = clampedOut - clampedIn;

    const drift::Project before = m_project;
    clip.srcIn = clampedIn;
    clip.srcOut = clampedOut;
    clip.timelineDuration = newDuration;
    pushProjectEdit(before, QStringLiteral("Trim updated"));
    finishEdit(QStringLiteral("Trim updated"));
}

void AppController::duplicateSelectedClip()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;

    drift::Track &track = m_project.tracks()[m_selectedTrack];
    if (m_selectedClip >= track.clips.size())
        return;

    const drift::Project before = m_project;
    const drift::Clip original = track.clips.at(m_selectedClip);
    drift::Clip copy = original;
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.timelineStart = drift::resolveClipStart(
        m_project, track, -1, original.timelineEnd(), original.timelineDuration, m_snapEnabled, m_playheadUs);

    track.clips.append(copy);
    pushProjectEdit(before, QStringLiteral("Clip duplicated"));
    finishEdit(QStringLiteral("Clip duplicated"));
    selectClip(m_selectedTrack, track.clips.size() - 1);
}

void AppController::alignSelectedClipLeft()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;

    drift::Track &track = m_project.tracks()[m_selectedTrack];
    if (m_selectedClip >= track.clips.size())
        return;

    const drift::Project before = m_project;
    drift::Clip &clip = track.clips[m_selectedClip];
    drift::TimeUs target = 0;
    for (int i = 0; i < track.clips.size(); ++i) {
        if (i == m_selectedClip)
            continue;
        const drift::Clip &other = track.clips.at(i);
        if (other.timelineEnd() <= clip.timelineStart)
            target = qMax(target, other.timelineEnd());
    }
    clip.timelineStart = drift::resolveClipStart(m_project, track, m_selectedClip, target, clip.timelineDuration,
                                                 m_snapEnabled, m_playheadUs);
    pushProjectEdit(before, QStringLiteral("Aligned left"));
    finishEdit(QStringLiteral("Aligned left"));
}

void AppController::alignSelectedClipRight()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;

    drift::Track &track = m_project.tracks()[m_selectedTrack];
    if (m_selectedClip >= track.clips.size())
        return;

    const drift::Project before = m_project;
    drift::Clip &clip = track.clips[m_selectedClip];
    drift::TimeUs nextStart = m_project.durationUs();
    for (int i = 0; i < track.clips.size(); ++i) {
        if (i == m_selectedClip)
            continue;
        const drift::Clip &other = track.clips.at(i);
        if (other.timelineStart >= clip.timelineEnd())
            nextStart = qMin(nextStart, other.timelineStart);
    }
    const drift::TimeUs newStart = qMax<drift::TimeUs>(0, nextStart - clip.timelineDuration);
    clip.timelineStart = drift::resolveClipStart(m_project, track, m_selectedClip, newStart, clip.timelineDuration,
                                                 m_snapEnabled, m_playheadUs);
    pushProjectEdit(before, QStringLiteral("Aligned right"));
    finishEdit(QStringLiteral("Aligned right"));
}

void AppController::moveClipToTrack(int trackIndex, int clipIndex, int newTrackIndex, double newStart)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;
    if (newTrackIndex < 0 || newTrackIndex >= m_project.tracks().size())
        return;

    drift::Track &fromTrack = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= fromTrack.clips.size())
        return;

    drift::Track &toTrack = m_project.tracks()[newTrackIndex];
    const drift::Clip clip = fromTrack.clips.at(clipIndex);
    if (!toTrack.allowsClipType(clip.type))
        return;

    const drift::Project before = m_project;
    fromTrack.clips.removeAt(clipIndex);
    drift::Clip moved = clip;
    moved.timelineStart = drift::resolveClipStart(m_project, toTrack, -1, drift::secondsToUs(newStart),
                                                  moved.timelineDuration, m_snapEnabled, m_playheadUs);
    toTrack.clips.append(moved);

    pushProjectEdit(before, QStringLiteral("Clip moved"));
    finishEdit(QStringLiteral("Clip moved"));
    selectClip(newTrackIndex, toTrack.clips.size() - 1);
}

void AppController::addTextClip(const QString &text, double atSeconds)
{
    const int trackIndex = drift::defaultTrackForClipType(m_project, drift::ClipType::Text);
    if (trackIndex < 0)
        return;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;

    const drift::Project before = m_project;
    drift::Track &track = m_project.tracks()[trackIndex];
    const drift::TimeUs startSeconds = atSeconds < 0.0 ? m_playheadUs : drift::secondsToUs(atSeconds);
    const drift::TimeUs start = drift::resolveClipStart(m_project, track, -1, startSeconds,
                                                        drift::kTextClipDurationUs, m_snapEnabled, m_playheadUs);

    drift::Clip clip;
    clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clip.type = drift::ClipType::Text;
    clip.name = trimmed.left(32);
    clip.textContent = trimmed;
    clip.timelineStart = start;
    clip.timelineDuration = drift::kTextClipDurationUs;
    clip.srcIn = 0;
    clip.srcOut = drift::kTextClipDurationUs;

    track.clips.append(clip);
    pushProjectEdit(before, QStringLiteral("Text clip added"));
    finishEdit(QStringLiteral("Text clip added"));
    selectClip(trackIndex, track.clips.size() - 1);
}

void AppController::setClipStart(int trackIndex, int clipIndex, double start)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    const drift::Project before = m_project;
    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs oldStart = clip.timelineStart;
    clip.timelineStart = drift::resolveClipStart(m_project, track, clipIndex, drift::secondsToUs(start),
                                                 clip.timelineDuration, m_snapEnabled, m_playheadUs);
    applyRippleShift(track, clipIndex, clip.timelineStart - oldStart);
    pushProjectEdit(before, QStringLiteral("Start updated"));
    finishEdit(QStringLiteral("Start updated"));
}

void AppController::setClipDuration(int trackIndex, int clipIndex, double duration)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    const drift::Project before = m_project;
    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs maxDuration = clip.type == drift::ClipType::Text
                                          ? drift::secondsToUs(300.0)
                                          : sourceDurationForClip(clip) - clip.srcIn;
    clip.timelineDuration = qBound(drift::kMinClipDurationUs, drift::secondsToUs(duration), maxDuration);
    clip.srcOut = clip.srcIn + clip.timelineDuration;
    pushProjectEdit(before, QStringLiteral("Duration updated"));
    finishEdit(QStringLiteral("Duration updated"));
}

void AppController::setClipTextContent(int trackIndex, int clipIndex, const QString &text)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    if (clip.type != drift::ClipType::Text)
        return;

    const drift::Project before = m_project;
    clip.textContent = text.trimmed();
    clip.name = clip.textContent.left(32);
    pushProjectEdit(before, QStringLiteral("Text updated"));
    finishEdit(QStringLiteral("Text updated"));
}

void AppController::setTextStyle(int trackIndex, int clipIndex, const QVariantMap &m)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    if (clip.type != drift::ClipType::Text)
        return;

    const drift::Project before = m_project;
    drift::TextStyle &s = clip.textStyle;
    if (m.contains(QStringLiteral("fontFamily")))
        s.fontFamily = m.value(QStringLiteral("fontFamily")).toString();
    if (m.contains(QStringLiteral("pixelSize")))
        s.pixelSize = m.value(QStringLiteral("pixelSize")).toInt();
    if (m.contains(QStringLiteral("color")))
        s.color = QColor(m.value(QStringLiteral("color")).toString());
    if (m.contains(QStringLiteral("bold")))
        s.bold = m.value(QStringLiteral("bold")).toBool();
    if (m.contains(QStringLiteral("italic")))
        s.italic = m.value(QStringLiteral("italic")).toBool();
    if (m.contains(QStringLiteral("align")))
        s.align = drift::textAlignFromString(m.value(QStringLiteral("align")).toString());
    if (m.contains(QStringLiteral("outlineWidth")))
        s.outlineWidth = m.value(QStringLiteral("outlineWidth")).toDouble();
    if (m.contains(QStringLiteral("outlineColor")))
        s.outlineColor = QColor(m.value(QStringLiteral("outlineColor")).toString());
    if (m.contains(QStringLiteral("boxEnabled")))
        s.boxEnabled = m.value(QStringLiteral("boxEnabled")).toBool();
    if (m.contains(QStringLiteral("boxColor")))
        s.boxColor = QColor(m.value(QStringLiteral("boxColor")).toString());
    if (m.contains(QStringLiteral("boxPadding")))
        s.boxPadding = m.value(QStringLiteral("boxPadding")).toDouble();
    pushProjectEdit(before, QStringLiteral("Edit text style"));
    finishEdit(QStringLiteral("Text style updated"));
}

void AppController::applyTextPreset(int trackIndex, int clipIndex, const QString &presetId)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    if (clip.type != drift::ClipType::Text)
        return;

    const drift::Project before = m_project;
    clip.textStyle = textStyleForPreset(presetId);
    pushProjectEdit(before, QStringLiteral("Apply text preset"));
    finishEdit(QStringLiteral("Text preset applied"));
}

void AppController::setClipBlendMode(int trackIndex, int clipIndex, const QString &mode)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    const drift::Project before = m_project;
    track.clips[clipIndex].blendMode = drift::blendModeFromString(mode);
    pushProjectEdit(before, QStringLiteral("Blend mode changed"));
    finishEdit(QStringLiteral("Blend mode updated"));
}

void AppController::setClipKeyframe(int trackIndex, int clipIndex, const QString &prop, double atSeconds,
                                    double value)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    drift::KeyframeTrack<double> *kt = trackForProp(clip, prop);
    if (!kt)
        return;

    const drift::Project before = m_project;
    const drift::TimeUs rel = qMax<drift::TimeUs>(0, drift::secondsToUs(atSeconds) - clip.timelineStart);
    kt->setKeyframe(rel, value);
    pushProjectEdit(before, QStringLiteral("Add keyframe"));
    finishEdit(QStringLiteral("Keyframe set"));
}

void AppController::removeClipKeyframe(int trackIndex, int clipIndex, const QString &prop, double atSeconds)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    drift::KeyframeTrack<double> *kt = trackForProp(clip, prop);
    if (!kt)
        return;

    const drift::Project before = m_project;
    const drift::TimeUs rel = qMax<drift::TimeUs>(0, drift::secondsToUs(atSeconds) - clip.timelineStart);
    kt->removeKeyframe(rel);
    pushProjectEdit(before, QStringLiteral("Remove keyframe"));
    finishEdit(QStringLiteral("Keyframe removed"));
}

QVariantList AppController::clipKeyframes(int trackIndex, int clipIndex, const QString &prop) const
{
    QVariantList out;
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return out;

    const drift::Track &track = m_project.tracks().at(trackIndex);
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return out;

    const drift::Clip &clip = track.clips.at(clipIndex);
    const drift::KeyframeTrack<double> *kt = trackForProp(clip, prop);
    if (!kt)
        return out;

    return keyframeListToVariant(*kt, clip.timelineStart);
}

void AppController::setKeyframeInterpolation(int trackIndex, int clipIndex, const QString &prop,
                                             const QString &mode)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    drift::KeyframeTrack<double> *kt = trackForProp(clip, prop);
    if (!kt)
        return;

    const drift::Project before = m_project;
    kt->setInterpolation(mode == QStringLiteral("hold") ? drift::Interpolation::Hold
                                                        : drift::Interpolation::Linear);
    pushProjectEdit(before, QStringLiteral("Keyframe interpolation changed"));
    finishEdit(QStringLiteral("Keyframe interpolation updated"));
}

QVariantList AppController::effectCatalog() const
{
    QVariantList out;
    for (const EffectDef &def : ::effectCatalog()) {
        QVariantList params;
        for (const EffectParamDef &p : def.params) {
            params.append(QVariantMap{
                {QStringLiteral("key"), p.key},
                {QStringLiteral("label"), p.label},
                {QStringLiteral("min"), p.min},
                {QStringLiteral("max"), p.max},
                {QStringLiteral("default"), p.def},
            });
        }
        out.append(QVariantMap{
            {QStringLiteral("id"), def.id},
            {QStringLiteral("label"), def.label},
            {QStringLiteral("category"), def.category},
            {QStringLiteral("params"), params},
        });
    }
    return out;
}

void AppController::addEffect(int trackIndex, int clipIndex, const QString &effectId)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    const EffectDef *def = effectDefForId(effectId);
    if (!def)
        return;

    drift::Effect effect;
    effect.name = def->filterName;
    effect.catalogId = def->id;
    for (auto it = def->fixedParams.constBegin(); it != def->fixedParams.constEnd(); ++it)
        effect.parameters.insert(it.key(), it.value());
    for (const EffectParamDef &p : def->params)
        effect.parameters.insert(p.key, p.def);

    const drift::Project before = m_project;
    track.clips[clipIndex].effects.append(effect);
    pushProjectEdit(before, QStringLiteral("Add effect"));
    finishEdit(QStringLiteral("Effect added"));
}

void AppController::removeEffect(int trackIndex, int clipIndex, int effectIndex)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    if (effectIndex < 0 || effectIndex >= clip.effects.size())
        return;

    const drift::Project before = m_project;
    clip.effects.removeAt(effectIndex);
    pushProjectEdit(before, QStringLiteral("Remove effect"));
    finishEdit(QStringLiteral("Effect removed"));
}

void AppController::setEffectParam(int trackIndex, int clipIndex, int effectIndex, const QString &key,
                                   double value)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    if (effectIndex < 0 || effectIndex >= clip.effects.size())
        return;

    const drift::Project before = m_project;
    clip.effects[effectIndex].parameters.insert(key, value);
    pushProjectEdit(before, QStringLiteral("Edit effect"));
    finishEdit(QStringLiteral("Effect updated"));
}

void AppController::setTrackMuted(int trackIndex, bool muted)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;
    if (m_project.tracks()[trackIndex].muted == muted)
        return;

    const drift::Project before = m_project;
    m_project.tracks()[trackIndex].muted = muted;
    pushProjectEdit(before, QStringLiteral("Track mute"));
    finishEdit(muted ? QStringLiteral("Track muted") : QStringLiteral("Track unmuted"));
}

void AppController::setTrackHidden(int trackIndex, bool hidden)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;
    if (m_project.tracks()[trackIndex].hidden == hidden)
        return;

    const drift::Project before = m_project;
    m_project.tracks()[trackIndex].hidden = hidden;
    pushProjectEdit(before, QStringLiteral("Track visibility"));
    finishEdit(hidden ? QStringLiteral("Track hidden") : QStringLiteral("Track shown"));
}

bool AppController::trackMuted(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return false;
    return m_project.tracks().at(trackIndex).muted;
}

bool AppController::trackHidden(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return false;
    return m_project.tracks().at(trackIndex).hidden;
}

QVariantList AppController::bookmarks() const
{
    QVariantList result;
    for (const drift::Bookmark &bookmark : m_project.bookmarks()) {
        result.append(QVariantMap{
            {QStringLiteral("seconds"), drift::usToSeconds(bookmark.timeUs)},
            {QStringLiteral("label"), bookmark.label},
        });
    }
    return result;
}

void AppController::addBookmark(double seconds, const QString &label)
{
    const drift::Project before = m_project;
    m_project.bookmarks().append({
        .timeUs = qMax<drift::TimeUs>(0, drift::secondsToUs(seconds)),
        .label = label.isEmpty() ? QStringLiteral("Bookmark") : label,
    });
    pushProjectEdit(before, QStringLiteral("Add bookmark"));
    finishEdit(QStringLiteral("Bookmark added"));
}

void AppController::removeBookmark(int index)
{
    if (index < 0 || index >= m_project.bookmarks().size())
        return;

    const drift::Project before = m_project;
    m_project.bookmarks().removeAt(index);
    pushProjectEdit(before, QStringLiteral("Remove bookmark"));
    finishEdit(QStringLiteral("Bookmark removed"));
}

void AppController::goToBookmark(int index)
{
    if (index < 0 || index >= m_project.bookmarks().size())
        return;
    setPlayheadUs(m_project.bookmarks().at(index).timeUs);
}

void AppController::freezeFrameAtPlayhead()
{
    const QVariantMap clip = activeVideoClipAtPlayhead();
    if (clip.isEmpty() || clip.value(QStringLiteral("kind")).toString() != QStringLiteral("video")) {
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

    const int trackIndex = drift::defaultTrackForClipType(m_project, drift::ClipType::Image);
    if (trackIndex < 0)
        return;

    const drift::Project before = m_project;
    drift::Track &track = m_project.tracks()[trackIndex];
    const drift::TimeUs sourceTimeUs = drift::secondsToUs(sourceTime);
    const drift::TimeUs start = drift::resolveClipStart(m_project, track, -1, m_playheadUs,
                                                        drift::kImageClipDurationUs, m_snapEnabled, m_playheadUs);

    drift::Clip freezeClip;
    freezeClip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    freezeClip.type = drift::ClipType::Image;
    freezeClip.name = QStringLiteral("Freeze frame");
    freezeClip.path = path;
    freezeClip.thumbnailPath = thumb;
    freezeClip.filmstripPath = thumb;
    freezeClip.timelineStart = start;
    freezeClip.timelineDuration = drift::kImageClipDurationUs;
    freezeClip.srcIn = sourceTimeUs;
    freezeClip.srcOut = sourceTimeUs + drift::kImageClipDurationUs;

    track.clips.append(freezeClip);
    pushProjectEdit(before, QStringLiteral("Freeze frame added"));
    finishEdit(QStringLiteral("Freeze frame added"));
}

void AppController::undo()
{
    if (!m_undoStack.canUndo())
        return;
    m_undoStack.undo();
    setLastMessage(QStringLiteral("Undo"));
}

void AppController::redo()
{
    if (!m_undoStack.canRedo())
        return;
    m_undoStack.redo();
    setLastMessage(QStringLiteral("Redo"));
}

QVariantList AppController::waveformPeaks(const QString &path) const
{
    if (path.isEmpty())
        return {};

    const auto cached = m_waveformCache.constFind(path);
    if (cached != m_waveformCache.constEnd())
        return cached.value();

    if (!m_waveformPending.contains(path)) {
        m_waveformPending.insert(path);
        AppController *self = const_cast<AppController *>(this);
        (void)QtConcurrent::run([self, path] {
            const QVariantList peaks = MediaWaveform::peaks(path);
            QMetaObject::invokeMethod(
                self,
                [self, path, peaks] {
                    self->m_waveformCache.insert(path, peaks);
                    self->m_waveformPending.remove(path);
                    emit self->waveformReady(path);
                },
                Qt::QueuedConnection);
        });
    }

    return {};
}

void AppController::restoreFilmstripsAfterLoad()
{
    if (!m_assetLibrary)
        return;

    for (drift::Track &track : m_project.tracks()) {
        for (drift::Clip &clip : track.clips) {
            if (!clip.filmstripPath.isEmpty())
                continue;
            const int assetIndex = assetIndexForClip(clip);
            if (assetIndex >= 0) {
                m_assetLibrary->ensureMedia(assetIndex);
                clip.filmstripPath = m_assetLibrary->filmstripAt(assetIndex);
            }
            if (clip.filmstripPath.isEmpty())
                clip.filmstripPath = clip.thumbnailPath;
        }
    }
}

void AppController::saveProject(const QUrl &url)
{
    const QString path = url.toLocalFile();
    if (path.isEmpty()) {
        setLastMessage(QStringLiteral("Invalid save path"));
        return;
    }

    QJsonObject root = m_project.toJson();
    root.insert(QStringLiteral("playheadUs"), static_cast<double>(m_playheadUs));
    root.insert(QStringLiteral("snapEnabled"), m_snapEnabled);
    root.insert(QStringLiteral("rippleEnabled"), m_rippleEnabled);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastMessage(QStringLiteral("Failed to save project"));
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    setLastMessage(QStringLiteral("Project saved"));
}

void AppController::loadProject(const QUrl &url)
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
    QString error;
    m_project = drift::Project::fromJson(root, &error);
    if (m_assetLibrary)
        m_assetLibrary->setProject(&m_project);

    setPlaying(false);
    m_snapEnabled = root.value(QStringLiteral("snapEnabled")).toBool(true);
    m_rippleEnabled = root.value(QStringLiteral("rippleEnabled")).toBool(false);

    if (root.contains(QStringLiteral("playheadUs"))) {
        setPlayheadUs(static_cast<drift::TimeUs>(root.value(QStringLiteral("playheadUs")).toDouble()));
    } else {
        setPlayheadSeconds(root.value(QStringLiteral("playheadSeconds")).toDouble());
    }

    restoreFilmstripsAfterLoad();
    m_playback.setProject(&m_project);
    m_undoStack.clear();
    clearSelection();
    emit snapEnabledChanged();
    emit rippleEnabledChanged();
    emit tracksChanged();
    emit bookmarksChanged();
    emit projectNameChanged();
    setLastMessage(QStringLiteral("Project loaded"));
}

void AppController::exportProject(const QUrl &outputUrl)
{
    const QString outputPath = outputUrl.toLocalFile();
    if (outputPath.isEmpty()) {
        setLastMessage(QStringLiteral("Invalid export path"));
        emit exportFinished(false);
        return;
    }

    if (m_exportInProgress) {
        setLastMessage(QStringLiteral("Export already in progress"));
        return;
    }

    QList<ExportSegment> videoSegments;
    QList<ExportSegment> audioSegments;
    QList<ExportTextOverlay> textOverlays;

    for (const drift::Track &track : m_project.tracks()) {
        if (track.muted || track.hidden)
            continue;

        if (track.type == drift::TrackType::Video) {
            for (const drift::Clip &clip : track.clips) {
                if (clip.type != drift::ClipType::Video && clip.type != drift::ClipType::Image)
                    continue;

                videoSegments.append({
                    .path = clip.path,
                    .kind = drift::clipTypeToString(clip.type),
                    .timelineStart = drift::usToSeconds(clip.timelineStart),
                    .inPoint = drift::usToSeconds(clip.srcIn),
                    .duration = drift::usToSeconds(clip.timelineDuration),
                });
            }
        } else if (track.type == drift::TrackType::Audio) {
            for (const drift::Clip &clip : track.clips) {
                audioSegments.append({
                    .path = clip.path,
                    .kind = QStringLiteral("audio"),
                    .timelineStart = drift::usToSeconds(clip.timelineStart),
                    .inPoint = drift::usToSeconds(clip.srcIn),
                    .duration = drift::usToSeconds(clip.timelineDuration),
                });
            }
        } else if (track.type == drift::TrackType::Text) {
            for (const drift::Clip &clip : track.clips) {
                const QString text = clip.textContent.isEmpty() ? clip.name : clip.textContent;
                if (text.isEmpty())
                    continue;
                textOverlays.append({
                    .text = text,
                    .timelineStart = drift::usToSeconds(clip.timelineStart),
                    .duration = drift::usToSeconds(clip.timelineDuration),
                });
            }
        }
    }

    m_exportInProgress = true;
    emit exportInProgressChanged();
    setLastMessage(QStringLiteral("Exporting..."));

    const QList<ExportSegment> videoCopy = videoSegments;
    const QList<ExportSegment> audioCopy = audioSegments;
    const QList<ExportTextOverlay> textCopy = textOverlays;

    (void)QtConcurrent::run([this, outputPath, videoCopy, audioCopy, textCopy]() {
        QString error;
        const bool ok = TimelineExporter::exportTimeline(videoCopy, audioCopy, textCopy, outputPath, &error);
        QMetaObject::invokeMethod(this, [this, ok, error]() {
            m_exportInProgress = false;
            emit exportInProgressChanged();
            setLastMessage(ok ? QStringLiteral("Export complete") : error);
            emit exportFinished(ok);
        }, Qt::QueuedConnection);
    });
}
