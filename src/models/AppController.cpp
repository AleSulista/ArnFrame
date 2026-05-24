#include "AppController.h"

#include "AssetLibrary.h"
#include "core/Clip.h"
#include "core/TimelineOps.h"
#include "core/Transition.h"
#include "core/commands/ProjectCommands.h"
#include "engine/EffectCatalog.h"
#include "engine/Exporter.h"
#include "engine/MediaThumbnail.h"
#include "engine/MediaWaveform.h"

#include <QColor>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>
#include <QtMath>
#include <algorithm>
#include <climits>
#include <limits>

namespace {
QHash<QString, QString> defaultShortcuts();
}

AppController::AppController(AssetLibrary *assetLibrary, QObject *parent)
    : QObject(parent)
    , m_assetLibrary(assetLibrary)
{
    m_project.resetToDefaultTimeline();
    if (m_assetLibrary)
        m_assetLibrary->setProject(&m_project);

    m_timelineModel.setProject(&m_project);
    m_clipListModel.setProject(&m_project);

    // selectedClipData reflects the current clip's live values, so it must
    // refresh on both selection changes and any edit to the timeline (e.g. a
    // WYSIWYG preview drag emits only tracksChanged). This keeps the Clip
    // Properties panel in sync with the preview in both directions.
    connect(this, &AppController::selectionChanged, this, &AppController::selectedClipDataChanged);
    connect(this, &AppController::tracksChanged, this, &AppController::selectedClipDataChanged);

    m_undoStack.setUndoLimit(kMaxUndoSteps);
    connect(&m_undoStack, &QUndoStack::indexChanged, this, &AppController::undoStackChanged);
    connect(&m_undoStack, &QUndoStack::indexChanged, this, [this] {
        m_timelineModel.refresh();
        m_clipListModel.refresh();
        normalizeSelection();
        emit tracksChanged();
        emit bookmarksChanged();
        emit projectNameChanged();
        emit selectionChanged();
        emit backgroundChanged();
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
        emit selectedTransitionDataChanged();
    });
    connect(this, &AppController::selectionChanged, this, [this] {
        m_clipListModel.setTrackIndex(m_selectedTrack >= 0 ? m_selectedTrack : 0);
    });

    m_shortcuts = defaultShortcuts();
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        const QString stored = settings.value(it.key(), it.value()).toString();
        if (!stored.isEmpty())
            it.value() = stored;
    }
    settings.endGroup();
    m_guidesEnabled = settings.value(QStringLiteral("preview/guidesEnabled"), false).toBool();
    m_guideType = settings.value(QStringLiteral("preview/guideType"), QStringLiteral("thirds")).toString();
}

namespace {
QVariantMap transitionToMap(const drift::Track &track, const drift::Transition &t);
QVariantMap maskToMap(const drift::Mask &m);
drift::Mask maskFromMap(const QVariantMap &m);

int findTransitionPartnerIndex(const drift::Track &track, int fromIndex)
{
    if (fromIndex < 0 || fromIndex >= track.clips.size())
        return -1;

    const drift::Clip &fromClip = track.clips.at(fromIndex);
    int best = -1;
    drift::TimeUs bestStart = std::numeric_limits<drift::TimeUs>::max();
    for (int i = 0; i < track.clips.size(); ++i) {
        if (i == fromIndex)
            continue;
        const drift::Clip &candidate = track.clips.at(i);
        if (!drift::clipsEligibleForTransition(fromClip, candidate))
            continue;
        if (candidate.timelineStart < bestStart) {
            bestStart = candidate.timelineStart;
            best = i;
        }
    }
    return best;
}

void syncOverlapTransitions(drift::Project &project)
{
    for (drift::Track &track : project.tracks()) {
        if (track.type != drift::TrackType::Video && track.type != drift::TrackType::Shape)
            continue;

        QList<int> order;
        order.reserve(track.clips.size());
        for (int i = 0; i < track.clips.size(); ++i)
            order.append(i);
        std::sort(order.begin(), order.end(), [&track](int a, int b) {
            const drift::Clip &ca = track.clips.at(a);
            const drift::Clip &cb = track.clips.at(b);
            if (ca.timelineStart != cb.timelineStart)
                return ca.timelineStart < cb.timelineStart;
            return ca.id < cb.id;
        });

        for (int i = 0; i + 1 < order.size(); ++i) {
            const int fromIndex = order.at(i);
            const int toIndex = order.at(i + 1);
            const drift::Clip &fromClip = track.clips.at(fromIndex);
            const drift::Clip &toClip = track.clips.at(toIndex);
            if (!drift::clipsPhysicallyOverlap(fromClip, toClip))
                continue;

            const drift::TimeUs overlapUs = drift::physicalOverlapDurationUs(fromClip, toClip);
            if (overlapUs < drift::secondsToUs(0.05))
                continue;

            drift::Transition *existing = nullptr;
            for (drift::Transition &transition : track.transitions) {
                if (transition.fromClipId == fromClip.id && transition.toClipId == toClip.id) {
                    existing = &transition;
                    break;
                }
            }

            if (existing) {
                existing->durationUs = overlapUs;
                continue;
            }

            drift::Transition transition;
            transition.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            transition.fromClipId = fromClip.id;
            transition.toClipId = toClip.id;
            transition.kind = drift::TransitionKind::Crossfade;
            transition.durationUs = overlapUs;
            track.transitions.append(transition);
        }

        for (int i = track.transitions.size() - 1; i >= 0; --i) {
            const drift::Transition &transition = track.transitions.at(i);
            const drift::Clip *fromClip = drift::clipById(track, transition.fromClipId);
            const drift::Clip *toClip = drift::clipById(track, transition.toClipId);
            if (!fromClip || !toClip || !drift::clipsEligibleForTransition(*fromClip, *toClip))
                track.transitions.removeAt(i);
        }
    }
}

} // namespace

QVariantList AppController::tracks() const
{
    QVariantList result;
    result.reserve(m_project.tracks().size());

    for (const drift::Track &track : m_project.tracks()) {
        QVariantList clips;
        clips.reserve(track.clips.size());

        for (const drift::Clip &clip : track.clips)
            clips.append(clipToMap(clip));

        QVariantList transitions;
        transitions.reserve(track.transitions.size());
        for (const drift::Transition &transition : track.transitions)
            transitions.append(transitionToMap(track, transition));

        result.append(QVariantMap{
            {QStringLiteral("type"), drift::trackTypeToString(track.type)},
            {QStringLiteral("clips"), clips},
            {QStringLiteral("transitions"), transitions},
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

QVariantMap shapeStyleToMap(const drift::ShapeStyle &s)
{
    return {
        {QStringLiteral("kind"), drift::shapeKindToString(s.kind)},
        {QStringLiteral("fill"), s.fill.name(QColor::HexArgb)},
        {QStringLiteral("stroke"), s.stroke.name(QColor::HexArgb)},
        {QStringLiteral("strokeWidth"), s.strokeWidth},
    };
}

QVariantMap maskToMap(const drift::Mask &m)
{
    QVariantList points;
    for (const QPointF &pt : m.points)
        points.append(QVariantList{pt.x(), pt.y()});

    return {
        {QStringLiteral("shape"), drift::maskShapeToString(m.shape)},
        {QStringLiteral("x"), m.x},
        {QStringLiteral("y"), m.y},
        {QStringLiteral("w"), m.w},
        {QStringLiteral("h"), m.h},
        {QStringLiteral("rotation"), m.rotation},
        {QStringLiteral("feather"), m.feather},
        {QStringLiteral("invert"), m.invert},
        {QStringLiteral("points"), points},
    };
}

drift::Mask maskFromMap(const QVariantMap &m)
{
    drift::Mask mask;
    mask.shape = drift::maskShapeFromString(m.value(QStringLiteral("shape")).toString());
    mask.x = m.value(QStringLiteral("x"), mask.x).toDouble();
    mask.y = m.value(QStringLiteral("y"), mask.y).toDouble();
    mask.w = m.value(QStringLiteral("w"), mask.w).toDouble();
    mask.h = m.value(QStringLiteral("h"), mask.h).toDouble();
    mask.rotation = m.value(QStringLiteral("rotation"), mask.rotation).toDouble();
    mask.feather = m.value(QStringLiteral("feather"), mask.feather).toDouble();
    mask.invert = m.value(QStringLiteral("invert"), mask.invert).toBool();
    const QVariantList points = m.value(QStringLiteral("points")).toList();
    for (const QVariant &value : points) {
        const QVariantList pair = value.toList();
        if (pair.size() >= 2)
            mask.points.append(QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
    }
    return mask;
}

QVariantMap transitionToMap(const drift::Track &track, const drift::Transition &t)
{
    drift::TimeUs startUs = 0;
    drift::TimeUs endUs = 0;
    const bool hasWindow = drift::transitionWindow(track, t, startUs, endUs);
    const drift::Clip *fromClip = drift::clipById(track, t.fromClipId);
    const drift::Clip *toClip = drift::clipById(track, t.toClipId);
    const bool overlapping = fromClip && toClip && drift::clipsPhysicallyOverlap(*fromClip, *toClip);
    const drift::TimeUs durationUs = hasWindow ? (endUs - startUs) : t.durationUs;

    return {
        {QStringLiteral("id"), t.id},
        {QStringLiteral("fromClipId"), t.fromClipId},
        {QStringLiteral("toClipId"), t.toClipId},
        {QStringLiteral("kind"), drift::transitionKindToString(t.kind)},
        {QStringLiteral("duration"), drift::usToSeconds(durationUs)},
        {QStringLiteral("start"), hasWindow ? drift::usToSeconds(startUs) : 0.0},
        {QStringLiteral("end"), hasWindow ? drift::usToSeconds(endUs) : 0.0},
        {QStringLiteral("overlapping"), overlapping},
        {QStringLiteral("label"), drift::transitionKindLabel(t.kind)},
    };
}

QString stickerResourcePath(const QString &id)
{
    return QStringLiteral("qrc:/qt/qml/Drift/resources/stickers/%1.png").arg(id);
}

bool clipAcceptsPreviewTransform(const drift::Clip &clip)
{
    return clip.type == drift::ClipType::Shape || clip.type == drift::ClipType::Image
           || clip.type == drift::ClipType::Text || clip.type == drift::ClipType::Video;
}

double clipTransformValue(const drift::KeyframeTrack<double> &track, drift::TimeUs relative, double defaultValue)
{
    if (track.isEmpty())
        return defaultValue;
    return track.evaluateAt(relative);
}

drift::ShapeStyle shapeStyleForKind(const QString &shapeKind)
{
    drift::ShapeStyle style;
    style.kind = drift::shapeKindFromString(shapeKind);
    switch (style.kind) {
    case drift::ShapeKind::Rectangle:
        style.fill = QColor(0, 180, 255);
        break;
    case drift::ShapeKind::Square:
        style.fill = QColor(255, 120, 64);
        break;
    case drift::ShapeKind::Triangle:
        style.fill = QColor(255, 214, 10);
        break;
    case drift::ShapeKind::Pentagon:
        style.fill = QColor(160, 96, 255);
        break;
    case drift::ShapeKind::Hexagon:
        style.fill = QColor(80, 220, 140);
        break;
    }
    return style;
}

drift::KeyframeTrack<double> *trackForProp(drift::Clip &clip, const QString &prop)
{
    if (prop == QStringLiteral("opacity"))
        return &clip.opacity;
    if (prop == QStringLiteral("x"))
        return &clip.transformX;
    if (prop == QStringLiteral("y"))
        return &clip.transformY;
    if (prop == QStringLiteral("width"))
        return &clip.transformW;
    if (prop == QStringLiteral("height"))
        return &clip.transformH;
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
    const EffectPresetEntry *def = effectDefForId(effect.catalogId);
    QVariantList params;
    if (def) {
        for (const drift::EffectParamSpec &paramDef : def->meta.parameters) {
            QVariant value = effect.parameters.value(paramDef.key);
            if (!value.isValid()) {
                value = paramDef.isBoolean ? QVariant(paramDef.defaultValue > 0.5)
                                           : QVariant(paramDef.defaultValue);
            }
            params.append(QVariantMap{
                {QStringLiteral("key"), paramDef.key},
                {QStringLiteral("label"), paramDef.label},
                {QStringLiteral("min"), paramDef.min},
                {QStringLiteral("max"), paramDef.max},
                {QStringLiteral("isBoolean"), paramDef.isBoolean},
                {QStringLiteral("value"), value},
            });
        }
    }
    return {
        {QStringLiteral("catalogId"), effect.catalogId},
        {QStringLiteral("label"), def ? def->meta.displayName : effect.name},
        {QStringLiteral("params"), params},
        {QStringLiteral("compositorOnly"), def ? def->meta.compositorOnly : false},
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

QVariantMap keyframeTrackToMap(const drift::KeyframeTrack<double> &track, drift::TimeUs timelineStart)
{
    return {
        {QStringLiteral("interpolation"),
         track.interpolation() == drift::Interpolation::Hold ? QStringLiteral("hold")
                                                            : QStringLiteral("linear")},
        {QStringLiteral("points"), keyframeListToVariant(track, timelineStart)},
    };
}

QVariantMap keyframesToMap(const drift::Clip &clip)
{
    return {
        {QStringLiteral("opacity"), keyframeTrackToMap(clip.opacity, clip.timelineStart)},
        {QStringLiteral("x"), keyframeTrackToMap(clip.transformX, clip.timelineStart)},
        {QStringLiteral("y"), keyframeTrackToMap(clip.transformY, clip.timelineStart)},
        {QStringLiteral("width"), keyframeTrackToMap(clip.transformW, clip.timelineStart)},
        {QStringLiteral("height"), keyframeTrackToMap(clip.transformH, clip.timelineStart)},
        {QStringLiteral("rotation"), keyframeTrackToMap(clip.rotation, clip.timelineStart)},
        {QStringLiteral("volume"), keyframeTrackToMap(clip.volume, clip.timelineStart)},
    };
}

void setClipLayoutPixels(drift::Clip &clip, double x, double y, double w, double h)
{
    clip.transformX = {};
    clip.transformY = {};
    clip.transformW = {};
    clip.transformH = {};
    clip.transformX.setKeyframe(0, x);
    clip.transformY.setKeyframe(0, y);
    clip.transformW.setKeyframe(0, qMax(1.0, w));
    clip.transformH.setKeyframe(0, qMax(1.0, h));
}

void fitClipLayoutToCanvas(drift::Clip &clip, int mediaW, int mediaH, int canvasW, int canvasH)
{
    canvasW = qMax(1, canvasW);
    canvasH = qMax(1, canvasH);
    if (mediaW <= 0 || mediaH <= 0) {
        setClipLayoutPixels(clip, 0, 0, canvasW, canvasH);
        return;
    }
    const double scale = qMin(static_cast<double>(canvasW) / mediaW, static_cast<double>(canvasH) / mediaH);
    setClipLayoutPixels(clip, 0, 0, mediaW * scale, mediaH * scale);
}

void applyAssetLayout(drift::Clip &clip, const QVariantMap &asset, int canvasW, int canvasH)
{
    int mediaW = asset.value(QStringLiteral("width")).toInt();
    int mediaH = asset.value(QStringLiteral("height")).toInt();
    const int rotation = asset.value(QStringLiteral("rotationDegrees")).toInt();
    if (rotation == 90 || rotation == 270)
        std::swap(mediaW, mediaH);
    fitClipLayoutToCanvas(clip, mediaW, mediaH, canvasW, canvasH);
}

void applyDefaultVisualLayout(drift::Clip &clip, int canvasW, int canvasH)
{
    canvasW = qMax(1, canvasW);
    canvasH = qMax(1, canvasH);
    if (clip.type == drift::ClipType::Shape) {
        const double w = canvasW * 0.30;
        const double h = canvasH * 0.20;
        setClipLayoutPixels(clip, 0, 0, w, h);
        return;
    }
    if (clip.type == drift::ClipType::Text) {
        setClipLayoutPixels(clip, 0, canvasH * 0.35, canvasW, canvasH * 0.30);
        return;
    }
    // Stickers / generic images without metadata: modest top-left box.
    const double side = qMin(canvasW, canvasH) * 0.25;
    setClipLayoutPixels(clip, 0, 0, side, side);
}

QHash<QString, QString> defaultShortcuts()
{
    return {
        {QStringLiteral("playPause"), QStringLiteral("Space")},
        {QStringLiteral("delete"), QStringLiteral("Delete")},
        {QStringLiteral("undo"), QStringLiteral("Ctrl+Z")},
        {QStringLiteral("redo"), QStringLiteral("Ctrl+Shift+Z")},
        {QStringLiteral("clearSelection"), QStringLiteral("Escape")},
        {QStringLiteral("duplicate"), QStringLiteral("Ctrl+D")},
        {QStringLiteral("split"), QStringLiteral("S")},
        {QStringLiteral("copy"), QStringLiteral("Ctrl+C")},
        {QStringLiteral("cut"), QStringLiteral("Ctrl+X")},
        {QStringLiteral("paste"), QStringLiteral("Ctrl+V")},
        {QStringLiteral("nudgeLeft"), QStringLiteral("Alt+Left")},
        {QStringLiteral("nudgeRight"), QStringLiteral("Alt+Right")},
        {QStringLiteral("toggleGuides"), QStringLiteral("G")},
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
        {QStringLiteral("shapeStyle"), shapeStyleToMap(clip.shapeStyle)},
        {QStringLiteral("blendMode"), drift::blendModeToString(clip.blendMode)},
        {QStringLiteral("speed"), clip.speed},
        {QStringLiteral("mask"), maskToMap(clip.mask)},
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

    QVariantMap data = clipAt(m_selectedTrack, m_selectedClip);
    if (!data.isEmpty())
        return data;

    // Primary indices can lag m_selection after structural edits; fall back.
    for (const QPair<int, int> &pair : m_selection) {
        data = clipAt(pair.first, pair.second);
        if (!data.isEmpty())
            return data;
    }
    return {};
}

QVariantList AppController::selectedClipEffects() const
{
    const QVariantMap clip = selectedClipData();
    return clip.value(QStringLiteral("effects")).toList();
}

QVariantList AppController::selection() const
{
    QVariantList out;
    for (const QPair<int, int> &pair : m_selection) {
        out.append(QVariantMap{
            {QStringLiteral("track"), pair.first},
            {QStringLiteral("clip"), pair.second},
        });
    }
    return out;
}

QVariantList AppController::actions() const
{
    auto action = [this](const QString &id, const QString &label) {
        return QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), label},
            {QStringLiteral("shortcut"), m_shortcuts.value(id)},
        };
    };

    return {
        action(QStringLiteral("playPause"), QStringLiteral("Play/Pause")),
        action(QStringLiteral("delete"), QStringLiteral("Delete selection")),
        action(QStringLiteral("undo"), QStringLiteral("Undo")),
        action(QStringLiteral("redo"), QStringLiteral("Redo")),
        action(QStringLiteral("copy"), QStringLiteral("Copy selection")),
        action(QStringLiteral("cut"), QStringLiteral("Cut selection")),
        action(QStringLiteral("paste"), QStringLiteral("Paste at playhead")),
        action(QStringLiteral("duplicate"), QStringLiteral("Duplicate selected clip")),
        action(QStringLiteral("split"), QStringLiteral("Split at playhead")),
        action(QStringLiteral("clearSelection"), QStringLiteral("Clear selection")),
        action(QStringLiteral("nudgeLeft"), QStringLiteral("Nudge selection left")),
        action(QStringLiteral("nudgeRight"), QStringLiteral("Nudge selection right")),
        action(QStringLiteral("toggleGuides"), QStringLiteral("Toggle guides")),
    };
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

void AppController::togglePlayback()
{
    setPlaying(!m_playback.isPlaying());
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

void AppController::setGuidesEnabled(bool enabled)
{
    if (m_guidesEnabled == enabled)
        return;
    m_guidesEnabled = enabled;
    QSettings settings;
    settings.setValue(QStringLiteral("preview/guidesEnabled"), m_guidesEnabled);
    emit guidesChanged();
}

void AppController::setGuideType(const QString &type)
{
    const QString normalized = type.trimmed().isEmpty() ? QStringLiteral("thirds") : type.trimmed();
    if (m_guideType == normalized)
        return;
    m_guideType = normalized;
    QSettings settings;
    settings.setValue(QStringLiteral("preview/guideType"), m_guideType);
    emit guidesChanged();
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
    const double speed = clip.value(QStringLiteral("speed"), 1.0).toDouble();
    const double effectiveSpeed = speed <= 0.0 ? 1.0 : speed;
    return inPoint + (playheadSeconds() - start) * effectiveSpeed;
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
    syncOverlapTransitions(m_project);
    normalizeSelection();
    // During playback the engine clock owns the playhead. Seeking here would
    // PlaybackClock::reset() and (historically) stop the clock while audio kept
    // pulling — freezing A/V at one spot after drops like adding an effect.
    if (!m_playback.isPlaying())
        m_playback.setPlayheadUs(m_playheadUs);
    emit tracksChanged();
    emit selectionChanged();
    emit selectedClipDataChanged();
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
    const drift::Project before = m_project;
    int trackIndex = drift::defaultTrackForClipType(m_project, clipType);
    if (trackIndex < 0)
        trackIndex = drift::ensureTrackForClipType(m_project, clipType, false);

    m_assetLibrary->ensureMedia(assetIndex);
    const QString thumbnailPath = m_assetLibrary->thumbnailAt(assetIndex);
    const QString filmstripPath = m_assetLibrary->filmstripAt(assetIndex);

    drift::Track &track = m_project.tracks()[trackIndex];
    if (!track.allowsClipType(clipType))
        return;

    const drift::TimeUs duration = clipDurationForAssetIndex(assetIndex);
    const drift::TimeUs start = drift::resolveClipStart(m_project, track, -1, m_playheadUs, duration, m_snapEnabled,
                                                        m_playheadUs);

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
    applyAssetLayout(clip, asset, m_project.width(), m_project.height());

    track.clips.append(clip);
    pushProjectEdit(before, QStringLiteral("Clip added"));
    finishEdit(QStringLiteral("Clip added"));
    selectClip(trackIndex, track.clips.size() - 1);
}

bool AppController::trackAcceptsAsset(int trackIndex, int assetIndex) const
{
    if (!m_assetLibrary || trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return false;

    const QVariantMap asset = m_assetLibrary->assetAt(assetIndex);
    if (asset.isEmpty())
        return false;

    const drift::ClipType clipType = drift::clipTypeFromString(asset.value(QStringLiteral("kind")).toString());
    return m_project.tracks().at(trackIndex).allowsClipType(clipType);
}

QString AppController::trackTypeForAsset(int assetIndex) const
{
    if (!m_assetLibrary)
        return QStringLiteral("video");

    const QVariantMap asset = m_assetLibrary->assetAt(assetIndex);
    if (asset.isEmpty())
        return QStringLiteral("video");

    const drift::ClipType clipType = drift::clipTypeFromString(asset.value(QStringLiteral("kind")).toString());
    return drift::trackTypeToString(drift::trackTypeForClipType(clipType));
}

void AppController::addClipFromAssetOnNewTrack(int assetIndex, double atSeconds)
{
    if (!m_assetLibrary)
        return;

    const QVariantMap asset = m_assetLibrary->assetAt(assetIndex);
    if (asset.isEmpty())
        return;

    const drift::ClipType clipType = drift::clipTypeFromString(asset.value(QStringLiteral("kind")).toString());
    const drift::Project before = m_project;
    const int trackIndex = drift::insertTrackAtTopForClipType(m_project, clipType);

    m_assetLibrary->ensureMedia(assetIndex);
    const QString thumbnailPath = m_assetLibrary->thumbnailAt(assetIndex);
    const QString filmstripPath = m_assetLibrary->filmstripAt(assetIndex);

    drift::Track &track = m_project.tracks()[trackIndex];
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
    applyAssetLayout(clip, asset, m_project.width(), m_project.height());

    track.clips.append(clip);
    pushProjectEdit(before, QStringLiteral("Clip added on new track"));
    finishEdit(QStringLiteral("Clip added on new track"));
    selectClip(trackIndex, track.clips.size() - 1);
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
    applyAssetLayout(clip, asset, m_project.width(), m_project.height());

    track.clips.append(clip);
    pushProjectEdit(before, QStringLiteral("Clip added"));
    finishEdit(QStringLiteral("Clip added"));
    selectClip(trackIndex, track.clips.size() - 1);
}

void AppController::selectClip(int trackIndex, int clipIndex)
{
    if (!isValidClipIndex(trackIndex, clipIndex))
        return;

    m_selectedTrack = trackIndex;
    m_selectedClip = clipIndex;
    m_selection = {qMakePair(trackIndex, clipIndex)};
    m_selectedTransitionTrack = -1;
    m_selectedTransitionLeftClip = -1;
    emit selectionChanged();
    emit selectedTransitionDataChanged();
}

void AppController::addToSelection(int trackIndex, int clipIndex)
{
    if (!isValidClipIndex(trackIndex, clipIndex))
        return;
    const QPair<int, int> pair(trackIndex, clipIndex);
    if (!m_selection.contains(pair))
        m_selection.append(pair);
    m_selectedTrack = trackIndex;
    m_selectedClip = clipIndex;
    emit selectionChanged();
}

void AppController::setSelection(const QVariantList &pairs)
{
    QList<QPair<int, int>> next;
    for (const QVariant &value : pairs) {
        const QVariantMap map = value.toMap();
        const int trackIndex = map.value(QStringLiteral("track")).toInt();
        const int clipIndex = map.value(QStringLiteral("clip")).toInt();
        if (!isValidClipIndex(trackIndex, clipIndex))
            continue;
        const QPair<int, int> pair(trackIndex, clipIndex);
        if (!next.contains(pair))
            next.append(pair);
    }
    m_selection = next;
    if (m_selection.isEmpty()) {
        m_selectedTrack = -1;
        m_selectedClip = -1;
    } else {
        m_selectedTrack = m_selection.constLast().first;
        m_selectedClip = m_selection.constLast().second;
    }
    emit selectionChanged();
}

void AppController::clearSelection()
{
    if (m_selectedTrack < 0 && m_selectedClip < 0 && m_selection.isEmpty() && m_selectedTransitionTrack < 0)
        return;

    m_selectedTrack = -1;
    m_selectedClip = -1;
    m_selection.clear();
    m_selectedTransitionTrack = -1;
    m_selectedTransitionLeftClip = -1;
    emit selectionChanged();
    emit selectedTransitionDataChanged();
}

void AppController::deleteSelectedClip()
{
    if (m_selection.isEmpty() && m_selectedTrack >= 0 && m_selectedClip >= 0)
        m_selection = {qMakePair(m_selectedTrack, m_selectedClip)};
    if (m_selection.isEmpty())
        return;

    const drift::Project before = m_project;
    QList<QPair<int, int>> pairs = m_selection;
    QSet<QString> removedClipIds;
    std::sort(pairs.begin(), pairs.end(), [](const QPair<int, int> &a, const QPair<int, int> &b) {
        if (a.first != b.first)
            return a.first > b.first;
        return a.second > b.second;
    });
    for (const QPair<int, int> &pair : pairs) {
        if (!isValidClipIndex(pair.first, pair.second))
            continue;
        removedClipIds.insert(m_project.tracks().at(pair.first).clips.at(pair.second).id);
        m_project.tracks()[pair.first].clips.removeAt(pair.second);
    }
    for (drift::Track &track : m_project.tracks()) {
        for (int i = track.transitions.size() - 1; i >= 0; --i) {
            const drift::Transition &transition = track.transitions.at(i);
            if (removedClipIds.contains(transition.fromClipId) || removedClipIds.contains(transition.toClipId))
                track.transitions.removeAt(i);
        }
    }
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

    const QPair<int, int> requested(trackIndex, clipIndex);
    QList<QPair<int, int>> targets = m_selection.contains(requested) ? m_selection
                                                                      : QList<QPair<int, int>>{requested};
    const drift::Project before = m_project;
    const drift::TimeUs desiredUs = drift::secondsToUs(newStart);
    const drift::TimeUs baseUs = m_project.tracks().at(trackIndex).clips.at(clipIndex).timelineStart;
    const drift::TimeUs delta = desiredUs - baseUs;
    for (const QPair<int, int> &pair : targets) {
        if (!isValidClipIndex(pair.first, pair.second))
            continue;
        drift::Clip &clip = m_project.tracks()[pair.first].clips[pair.second];
        clip.timelineStart = qMax<drift::TimeUs>(0, clip.timelineStart + delta);
    }
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

            const drift::TimeUs sourceOffset =
                static_cast<drift::TimeUs>(static_cast<double>(offset) * clip.effectiveSpeed());

            drift::Clip tail = clip;
            tail.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            tail.timelineStart = m_playheadUs;
            tail.srcIn = clip.srcIn + sourceOffset;
            tail.timelineDuration = clip.timelineDuration - offset;

            clip.timelineDuration = offset;
            clip.srcOut = clip.srcIn + sourceOffset;

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
        const drift::TimeUs sourceDelta =
            static_cast<drift::TimeUs>(static_cast<double>(delta) * clip.effectiveSpeed());
        if (clip.srcIn + sourceDelta > clip.srcOut - drift::kMinClipDurationUs)
            return;

        clip.timelineStart += delta;
        clip.srcIn += sourceDelta;
        clip.timelineDuration -= delta;
    } else {
        const drift::TimeUs extendBy = -delta;
        const drift::TimeUs sourceExtend =
            static_cast<drift::TimeUs>(static_cast<double>(extendBy) * clip.effectiveSpeed());
        if (sourceExtend > clip.srcIn)
            return;

        clip.timelineStart = snappedStart;
        clip.srcIn -= sourceExtend;
        clip.timelineDuration += extendBy;
    }

    syncOverlapTransitions(m_project);
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
    clip.srcOut = qMin(clip.srcIn + clip.sourceSpanUs(), sourceDurationForClip(clip));
    syncOverlapTransitions(m_project);
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
    const double speed = clip.effectiveSpeed();

    const drift::Project before = m_project;
    clip.srcIn = clampedIn;
    clip.srcOut = clampedOut;
    clip.timelineDuration = static_cast<drift::TimeUs>(llround(static_cast<double>(newDuration) / speed));
    clip.timelineDuration = qMax(clip.timelineDuration, drift::kMinClipDurationUs);
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
    splitSelectedClipLeft();
}

void AppController::alignSelectedClipRight()
{
    splitSelectedClipRight();
}

void AppController::splitSelectedClipLeft()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;

    drift::Track &track = m_project.tracks()[m_selectedTrack];
    if (m_selectedClip >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[m_selectedClip];
    if (!clip.containsTime(m_playheadUs) || m_playheadUs == clip.timelineStart)
        return;

    const drift::TimeUs offset = m_playheadUs - clip.timelineStart;
    if (offset < drift::kMinClipDurationUs || clip.timelineDuration - offset < drift::kMinClipDurationUs)
        return;

    const drift::Project before = m_project;

    drift::Clip right = clip;
    right.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    right.timelineStart = m_playheadUs;
    right.srcIn = clip.srcIn + offset;
    right.timelineDuration = clip.timelineDuration - offset;

    track.clips[m_selectedClip] = right;

    pushProjectEdit(before, QStringLiteral("Split left"));
    finishEdit(QStringLiteral("Split left"));
    selectClip(m_selectedTrack, m_selectedClip);
}

void AppController::splitSelectedClipRight()
{
    if (m_selectedTrack < 0 || m_selectedClip < 0)
        return;

    drift::Track &track = m_project.tracks()[m_selectedTrack];
    if (m_selectedClip >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[m_selectedClip];
    if (!clip.containsTime(m_playheadUs) || m_playheadUs == clip.timelineEnd())
        return;

    const drift::TimeUs offset = m_playheadUs - clip.timelineStart;
    if (offset < drift::kMinClipDurationUs || clip.timelineDuration - offset < drift::kMinClipDurationUs)
        return;

    const drift::Project before = m_project;
    const drift::TimeUs sourceOffset =
        static_cast<drift::TimeUs>(static_cast<double>(offset) * clip.effectiveSpeed());

    clip.timelineDuration = offset;
    clip.srcOut = clip.srcIn + sourceOffset;

    pushProjectEdit(before, QStringLiteral("Split right"));
    finishEdit(QStringLiteral("Split right"));
    selectClip(m_selectedTrack, m_selectedClip);
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
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;

    const drift::Project before = m_project;
    const int trackIndex = drift::ensureTrackForClipType(m_project, drift::ClipType::Text, true);
    if (trackIndex < 0)
        return;

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
    applyDefaultVisualLayout(clip, m_project.width(), m_project.height());

    track.clips.append(clip);
    pushProjectEdit(before, QStringLiteral("Text clip added"));
    finishEdit(QStringLiteral("Text clip added"));
    selectClip(trackIndex, track.clips.size() - 1);
}

QVariantList AppController::builtinStickers() const
{
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("star")},
                    {QStringLiteral("label"), QStringLiteral("Star")},
                    {QStringLiteral("path"), stickerResourcePath(QStringLiteral("star"))}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("heart")},
                    {QStringLiteral("label"), QStringLiteral("Heart")},
                    {QStringLiteral("path"), stickerResourcePath(QStringLiteral("heart"))}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("arrow")},
                    {QStringLiteral("label"), QStringLiteral("Arrow")},
                    {QStringLiteral("path"), stickerResourcePath(QStringLiteral("arrow"))}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("circle")},
                    {QStringLiteral("label"), QStringLiteral("Circle")},
                    {QStringLiteral("path"), stickerResourcePath(QStringLiteral("circle"))}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("check")},
                    {QStringLiteral("label"), QStringLiteral("Check")},
                    {QStringLiteral("path"), stickerResourcePath(QStringLiteral("check"))}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("fire")},
                    {QStringLiteral("label"), QStringLiteral("Fire")},
                    {QStringLiteral("path"), stickerResourcePath(QStringLiteral("fire"))}},
    };
}

QVariantList AppController::builtinShapes() const
{
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("rectangle")},
                    {QStringLiteral("label"), QStringLiteral("Rectangle")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("square")}, {QStringLiteral("label"), QStringLiteral("Square")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("triangle")},
                    {QStringLiteral("label"), QStringLiteral("Triangle")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("pentagon")},
                    {QStringLiteral("label"), QStringLiteral("Pentagon")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hexagon")}, {QStringLiteral("label"), QStringLiteral("Hexagon")}},
    };
}

void AppController::addShapeClip(const QString &shapeKind, double atSeconds)
{
    addShapeClipAt(shapeKind, -1, atSeconds);
}

void AppController::addShapeClipAt(const QString &shapeKind, int trackIndex, double atSeconds)
{
    const drift::ShapeStyle style = shapeStyleForKind(shapeKind);
    const drift::Project before = m_project;

    int target = trackIndex;
    if (target < 0 || target >= m_project.tracks().size()
        || !m_project.tracks().at(target).allowsClipType(drift::ClipType::Shape)) {
        target = drift::ensureTrackForClipType(m_project, drift::ClipType::Shape, true);
    }
    if (target < 0)
        return;

    drift::Track &track = m_project.tracks()[target];
    const drift::TimeUs startSeconds = atSeconds < 0.0 ? m_playheadUs : drift::secondsToUs(atSeconds);
    const drift::TimeUs start = drift::resolveClipStart(m_project, track, -1, startSeconds,
                                                        drift::kImageClipDurationUs, m_snapEnabled, m_playheadUs);

    drift::Clip clip;
    clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clip.type = drift::ClipType::Shape;
    clip.name = drift::shapeKindToString(style.kind);
    clip.shapeStyle = style;
    clip.timelineStart = start;
    clip.timelineDuration = drift::kImageClipDurationUs;
    clip.srcIn = 0;
    clip.srcOut = drift::kImageClipDurationUs;
    applyDefaultVisualLayout(clip, m_project.width(), m_project.height());

    track.clips.append(clip);
    pushProjectEdit(before, QStringLiteral("Shape added"));
    finishEdit(QStringLiteral("Shape added"));
    selectClip(target, track.clips.size() - 1);
}

void AppController::addStickerClip(const QString &stickerId, double atSeconds)
{
    QString path;
    QString label;
    for (const QVariant &item : builtinStickers()) {
        const QVariantMap sticker = item.toMap();
        if (sticker.value(QStringLiteral("id")).toString() == stickerId) {
            path = sticker.value(QStringLiteral("path")).toString();
            label = sticker.value(QStringLiteral("label")).toString();
            break;
        }
    }
    if (path.isEmpty())
        return;

    const drift::Project before = m_project;
    const int trackIndex = drift::ensureTrackForClipType(m_project, drift::ClipType::Image, true);
    if (trackIndex < 0)
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    const drift::TimeUs startSeconds = atSeconds < 0.0 ? m_playheadUs : drift::secondsToUs(atSeconds);
    const drift::TimeUs start = drift::resolveClipStart(m_project, track, -1, startSeconds,
                                                        drift::kImageClipDurationUs, m_snapEnabled, m_playheadUs);

    drift::Clip clip;
    clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clip.type = drift::ClipType::Image;
    clip.name = label.isEmpty() ? stickerId : label;
    clip.path = path;
    clip.thumbnailPath = path;
    clip.filmstripPath = path;
    clip.timelineStart = start;
    clip.timelineDuration = drift::kImageClipDurationUs;
    clip.srcIn = 0;
    clip.srcOut = drift::kImageClipDurationUs;
    applyDefaultVisualLayout(clip, m_project.width(), m_project.height());

    track.clips.append(clip);
    pushProjectEdit(before, QStringLiteral("Sticker added"));
    finishEdit(QStringLiteral("Sticker added"));
    selectClip(trackIndex, track.clips.size() - 1);
}

QVariantList AppController::previewClipsAtPlayhead() const
{
    QVariantList out;
    const int canvasWidth = m_project.width();
    const int canvasHeight = m_project.height();
    if (canvasWidth <= 0 || canvasHeight <= 0)
        return out;

    const QList<drift::Track> &tracks = m_project.tracks();
    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        const drift::Track &track = tracks.at(trackIndex);
        if (track.hidden)
            continue;
        if (track.type != drift::TrackType::Video && track.type != drift::TrackType::Shape
            && track.type != drift::TrackType::Text)
            continue;

        for (int clipIndex = 0; clipIndex < track.clips.size(); ++clipIndex) {
            const drift::Clip &clip = track.clips.at(clipIndex);
            if (!clip.containsTime(m_playheadUs) || !clipAcceptsPreviewTransform(clip))
                continue;

            const drift::TimeUs relative = m_playheadUs - clip.timelineStart;
            const double x = clipTransformValue(clip.transformX, relative, 0.0);
            const double y = clipTransformValue(clip.transformY, relative, 0.0);
            const double w = clipTransformValue(clip.transformW, relative, static_cast<double>(canvasWidth));
            const double h = clipTransformValue(clip.transformH, relative, static_cast<double>(canvasHeight));
            const double rotation = clipTransformValue(clip.rotation, relative, 0.0);

            out.append(QVariantMap{
                {QStringLiteral("track"), trackIndex},
                {QStringLiteral("clip"), clipIndex},
                {QStringLiteral("kind"), drift::clipTypeToString(clip.type)},
                {QStringLiteral("name"), clip.name},
                {QStringLiteral("x"), x},
                {QStringLiteral("y"), y},
                {QStringLiteral("width"), w},
                {QStringLiteral("height"), h},
                {QStringLiteral("rotation"), rotation},
                {QStringLiteral("canvasWidth"), canvasWidth},
                {QStringLiteral("canvasHeight"), canvasHeight},
            });
        }
    }
    return out;
}

int AppController::projectWidth() const
{
    return m_project.width();
}

int AppController::projectHeight() const
{
    return m_project.height();
}

int AppController::projectFps() const
{
    return m_project.fps();
}

void AppController::setProjectResolution(int width, int height)
{
    setProjectSetup(width, height, m_project.fps());
}

void AppController::setProjectSetup(int width, int height, int fps)
{
    width = qBound(16, width, 7680);
    height = qBound(16, height, 4320);
    fps = qBound(1, fps, 240);
    if (m_project.width() == width && m_project.height() == height && m_project.fps() == fps)
        return;

    const drift::Project before = m_project;
    m_project.setResolution(width, height);
    m_project.setFps(fps);
    pushProjectEdit(before, QStringLiteral("Project setup"));
    finishEdit(QStringLiteral("Project setup updated"));
}

QVariantMap AppController::background() const
{
    const drift::Background &bg = m_project.background();
    QVariantMap map;
    map.insert(QStringLiteral("kind"),
               bg.kind == drift::BackgroundKind::Blur ? QStringLiteral("blur") : QStringLiteral("color"));
    map.insert(QStringLiteral("color"), bg.color.name(QColor::HexArgb));
    map.insert(QStringLiteral("blurStrength"), bg.blurStrength);
    return map;
}

void AppController::setBackground(const QVariantMap &background)
{
    drift::Background bg = m_project.background();
    if (background.contains(QStringLiteral("kind"))) {
        bg.kind = background.value(QStringLiteral("kind")).toString() == QStringLiteral("blur")
                      ? drift::BackgroundKind::Blur
                      : drift::BackgroundKind::Color;
    }
    if (background.contains(QStringLiteral("color"))) {
        const QColor color(background.value(QStringLiteral("color")).toString());
        if (color.isValid())
            bg.color = color;
    }
    if (background.contains(QStringLiteral("blurStrength")))
        bg.blurStrength = qBound(0.0, background.value(QStringLiteral("blurStrength")).toDouble(), 200.0);

    const drift::Background &current = m_project.background();
    if (current.kind == bg.kind && current.color == bg.color
        && qFuzzyCompare(current.blurStrength + 1.0, bg.blurStrength + 1.0))
        return;

    const drift::Project before = m_project;
    m_project.setBackground(bg);
    pushProjectEdit(before, QStringLiteral("Change background"));
    finishEdit(QStringLiteral("Background updated"));
    emit backgroundChanged();
    emitPreviewFrame();
}

bool AppController::timelineHasVisualClips() const
{
    for (const drift::Track &track : m_project.tracks()) {
        for (const drift::Clip &clip : track.clips) {
            if (clip.type == drift::ClipType::Video || clip.type == drift::ClipType::Image
                || clip.type == drift::ClipType::Text || clip.type == drift::ClipType::Shape) {
                return true;
            }
        }
    }
    return false;
}

bool AppController::shouldConfigureProjectForAsset(int assetIndex) const
{
    if (!m_assetLibrary)
        return false;
    const QVariantMap asset = m_assetLibrary->assetAt(assetIndex);
    if (asset.isEmpty())
        return false;
    const QString kind = asset.value(QStringLiteral("kind")).toString();
    if (kind != QStringLiteral("video") && kind != QStringLiteral("image"))
        return false;

    // Offer setup only for the first video/image clip (text/shapes alone don't count).
    for (const drift::Track &track : m_project.tracks()) {
        for (const drift::Clip &clip : track.clips) {
            if (clip.type == drift::ClipType::Video || clip.type == drift::ClipType::Image)
                return false;
        }
    }
    return true;
}

QVariantMap AppController::suggestedProjectSetupForAsset(int assetIndex) const
{
    QVariantMap out{
        {QStringLiteral("width"), m_project.width()},
        {QStringLiteral("height"), m_project.height()},
        {QStringLiteral("fps"), m_project.fps()},
        {QStringLiteral("aspect"), QStringLiteral("custom")},
    };
    if (!m_assetLibrary)
        return out;

    const QVariantMap asset = m_assetLibrary->assetAt(assetIndex);
    if (asset.isEmpty())
        return out;

    int w = asset.value(QStringLiteral("width")).toInt();
    int h = asset.value(QStringLiteral("height")).toInt();
    const int rotation = asset.value(QStringLiteral("rotationDegrees")).toInt();
    if (rotation == 90 || rotation == 270)
        std::swap(w, h);
    if (w > 0 && h > 0) {
        out.insert(QStringLiteral("width"), w);
        out.insert(QStringLiteral("height"), h);
        const double ratio = static_cast<double>(w) / static_cast<double>(h);
        if (qAbs(ratio - 16.0 / 9.0) < 0.02)
            out.insert(QStringLiteral("aspect"), QStringLiteral("16:9"));
        else if (qAbs(ratio - 9.0 / 16.0) < 0.02)
            out.insert(QStringLiteral("aspect"), QStringLiteral("9:16"));
        else if (qAbs(ratio - 4.0 / 3.0) < 0.02)
            out.insert(QStringLiteral("aspect"), QStringLiteral("4:3"));
        else if (qAbs(ratio - 1.0) < 0.02)
            out.insert(QStringLiteral("aspect"), QStringLiteral("1:1"));
        else
            out.insert(QStringLiteral("aspect"), QStringLiteral("source"));
    }
    const double fps = asset.value(QStringLiteral("fps")).toDouble();
    if (fps >= 1.0)
        out.insert(QStringLiteral("fps"), qRound(fps));
    out.insert(QStringLiteral("name"), asset.value(QStringLiteral("name")).toString());
    out.insert(QStringLiteral("kind"), asset.value(QStringLiteral("kind")).toString());
    return out;
}

void AppController::beginPreviewDrag(const QString &undoText)
{
    m_previewDragBefore = m_project;
    m_previewDragActive = true;
    m_previewDragText = undoText.isEmpty() ? QStringLiteral("Edit clip") : undoText;
}

void AppController::emitPreviewFrame()
{
    // Same rule as finishEdit: never seek the live clock for a preview refresh.
    if (!m_playback.isPlaying())
        m_playback.setPlayheadUs(m_playheadUs);
    emit tracksChanged(); // also notifies selectedClipDataChanged via connection
    m_playback.refreshFrame();
}

void AppController::previewSetClipPosition(int trackIndex, int clipIndex, double xPixels, double yPixels)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    if (!m_previewDragActive)
        beginPreviewDrag(QStringLiteral("Move clip"));

    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs relative = qMax<drift::TimeUs>(0, m_playheadUs - clip.timelineStart);
    clip.transformX.setKeyframe(relative, xPixels);
    clip.transformY.setKeyframe(relative, yPixels);
    emitPreviewFrame();
}

void AppController::previewSetClipSize(int trackIndex, int clipIndex, double widthPixels, double heightPixels)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    if (!m_previewDragActive)
        beginPreviewDrag(QStringLiteral("Resize clip"));

    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs relative = qMax<drift::TimeUs>(0, m_playheadUs - clip.timelineStart);
    clip.transformW.setKeyframe(relative, qMax(1.0, widthPixels));
    clip.transformH.setKeyframe(relative, qMax(1.0, heightPixels));
    emitPreviewFrame();
}

void AppController::previewSetClipRect(int trackIndex, int clipIndex, double xPixels, double yPixels,
                                       double widthPixels, double heightPixels)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    if (!m_previewDragActive)
        beginPreviewDrag(QStringLiteral("Transform clip"));

    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs relative = qMax<drift::TimeUs>(0, m_playheadUs - clip.timelineStart);
    clip.transformX.setKeyframe(relative, xPixels);
    clip.transformY.setKeyframe(relative, yPixels);
    clip.transformW.setKeyframe(relative, qMax(1.0, widthPixels));
    clip.transformH.setKeyframe(relative, qMax(1.0, heightPixels));
    emitPreviewFrame();
}

void AppController::previewSetClipRotation(int trackIndex, int clipIndex, double degrees)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    if (!m_previewDragActive)
        beginPreviewDrag(QStringLiteral("Rotate clip"));

    drift::Clip &clip = track.clips[clipIndex];
    const drift::TimeUs relative = qMax<drift::TimeUs>(0, m_playheadUs - clip.timelineStart);
    clip.rotation.setKeyframe(relative, degrees);
    emitPreviewFrame();
}

void AppController::previewSetClipKeyframe(int trackIndex, int clipIndex, const QString &prop,
                                           double atSeconds, double value)
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

    if (!m_previewDragActive)
        beginPreviewDrag(QStringLiteral("Edit keyframe"));

    const drift::TimeUs rel = qMax<drift::TimeUs>(0, drift::secondsToUs(atSeconds) - clip.timelineStart);
    kt->setKeyframe(rel, value);
    emitPreviewFrame();
}

void AppController::previewSetEffectParam(int trackIndex, int clipIndex, int effectIndex,
                                          const QString &key, double value)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    if (effectIndex < 0 || effectIndex >= clip.effects.size())
        return;
    if (key.isEmpty())
        return;

    if (!m_previewDragActive)
        beginPreviewDrag(QStringLiteral("Edit effect"));

    const EffectPresetEntry *def = effectDefForId(clip.effects[effectIndex].catalogId);
    bool asBoolean = false;
    if (def) {
        for (const drift::EffectParamSpec &param : def->meta.parameters) {
            if (param.key == key) {
                asBoolean = param.isBoolean;
                break;
            }
        }
    }
    if (asBoolean)
        clip.effects[effectIndex].parameters.insert(key, value > 0.5);
    else
        clip.effects[effectIndex].parameters.insert(key, value);
    emitPreviewFrame();
}

void AppController::previewSetClipSpeed(int trackIndex, int clipIndex, double speed)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    if (clip.type != drift::ClipType::Video && clip.type != drift::ClipType::Audio)
        return;

    if (!m_previewDragActive)
        beginPreviewDrag(QStringLiteral("Speed changed"));

    clip.speed = qBound(0.25, speed, 4.0);
    clip.syncSrcOutFromSpeed(sourceDurationForClip(clip));
    emitPreviewFrame();
}

void AppController::previewSetClipMask(int trackIndex, int clipIndex, const QVariantMap &maskMap)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    if (!m_previewDragActive)
        beginPreviewDrag(QStringLiteral("Mask changed"));

    track.clips[clipIndex].mask = maskFromMap(maskMap);
    emitPreviewFrame();
}

void AppController::commitPreviewDrag()
{
    if (!m_previewDragActive)
        return;

    const QString text = m_previewDragText.isEmpty() ? QStringLiteral("Edit clip") : m_previewDragText;
    m_undoStack.push(new drift::ProjectSnapshotCommand(&m_project, m_previewDragBefore, m_project, text));
    m_previewDragActive = false;
    finishEdit(text);
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
    const drift::TimeUs maxDuration = clip.type == drift::ClipType::Text || clip.type == drift::ClipType::Shape
                                          ? drift::secondsToUs(300.0)
                                          : sourceDurationForClip(clip) - clip.srcIn;
    clip.timelineDuration = qBound(drift::kMinClipDurationUs, drift::secondsToUs(duration), maxDuration);
    clip.srcOut = qMin(clip.srcIn + clip.sourceSpanUs(), sourceDurationForClip(clip));
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

void AppController::setClipSpeed(int trackIndex, int clipIndex, double speed)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    if (clip.type != drift::ClipType::Video && clip.type != drift::ClipType::Audio)
        return;

    const drift::Project before = m_project;
    clip.speed = qBound(0.25, speed, 4.0);
    clip.syncSrcOutFromSpeed(sourceDurationForClip(clip));
    pushProjectEdit(before, QStringLiteral("Speed changed"));
    finishEdit(QStringLiteral("Clip speed updated"));
}

void AppController::setClipMask(int trackIndex, int clipIndex, const QVariantMap &maskMap)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    const drift::Project before = m_project;
    track.clips[clipIndex].mask = maskFromMap(maskMap);
    pushProjectEdit(before, QStringLiteral("Mask changed"));
    finishEdit(QStringLiteral("Clip mask updated"));
}

void AppController::addTransition(int trackIndex, int clipIndex, const QString &kind, double durationSeconds)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (track.type != drift::TrackType::Video && track.type != drift::TrackType::Shape)
        return;

    const int partnerIndex = findTransitionPartnerIndex(track, clipIndex);
    if (partnerIndex < 0)
        return;

    const drift::Clip &fromClip = track.clips.at(clipIndex);
    const drift::Clip &toClip = track.clips.at(partnerIndex);
    const drift::TimeUs overlapUs = drift::physicalOverlapDurationUs(fromClip, toClip);
    const drift::TimeUs requestedUs = qMax<drift::TimeUs>(drift::secondsToUs(0.1), drift::secondsToUs(durationSeconds));
    const drift::TimeUs durationUs = overlapUs > 0 ? overlapUs : requestedUs;
    const drift::TransitionKind transitionKind = drift::transitionKindFromString(kind);

    for (drift::Transition &existing : track.transitions) {
        if (existing.fromClipId == fromClip.id && existing.toClipId == toClip.id) {
            const drift::Project before = m_project;
            existing.kind = transitionKind;
            existing.durationUs = durationUs;
            pushProjectEdit(before, QStringLiteral("Replace transition"));
            finishEdit(QStringLiteral("Transition updated"));
            selectTransition(trackIndex, clipIndex);
            return;
        }
    }

    drift::Transition transition;
    transition.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    transition.fromClipId = fromClip.id;
    transition.toClipId = toClip.id;
    transition.kind = transitionKind;
    transition.durationUs = durationUs;

    const drift::Project before = m_project;
    track.transitions.append(transition);
    pushProjectEdit(before, QStringLiteral("Add transition"));
    finishEdit(QStringLiteral("Transition added"));
    selectTransition(trackIndex, clipIndex);
}

void AppController::removeTransition(int trackIndex, const QString &transitionId)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    const drift::Project before = m_project;
    for (int i = 0; i < track.transitions.size(); ++i) {
        if (track.transitions.at(i).id != transitionId)
            continue;

        const drift::Transition transition = track.transitions.at(i);
        if (m_selectedTransitionTrack == trackIndex && m_selectedTransitionLeftClip >= 0) {
            const QString fromId = track.clips.value(m_selectedTransitionLeftClip).id;
            if (transition.fromClipId == fromId)
                clearTransitionSelection();
        }

        // Physical overlaps auto-sync a crossfade; separate the clips so removal sticks.
        drift::Clip *fromClip = nullptr;
        drift::Clip *toClip = nullptr;
        for (drift::Clip &clip : track.clips) {
            if (clip.id == transition.fromClipId)
                fromClip = &clip;
            else if (clip.id == transition.toClipId)
                toClip = &clip;
        }
        if (fromClip && toClip && drift::clipsPhysicallyOverlap(*fromClip, *toClip))
            toClip->timelineStart = fromClip->timelineEnd();

        track.transitions.removeAt(i);
        pushProjectEdit(before, QStringLiteral("Remove transition"));
        finishEdit(QStringLiteral("Transition removed"));
        return;
    }
}

void AppController::setTransitionDuration(int trackIndex, const QString &transitionId, double durationSeconds)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    const drift::Project before = m_project;
    for (drift::Transition &transition : track.transitions) {
        if (transition.id != transitionId)
            continue;

        const drift::TimeUs durationUs =
            qMax<drift::TimeUs>(drift::secondsToUs(0.1), drift::secondsToUs(durationSeconds));
        drift::Clip *fromClip = nullptr;
        drift::Clip *toClip = nullptr;
        for (drift::Clip &clip : track.clips) {
            if (clip.id == transition.fromClipId)
                fromClip = &clip;
            else if (clip.id == transition.toClipId)
                toClip = &clip;
        }

        if (fromClip && toClip && drift::clipsPhysicallyOverlap(*fromClip, *toClip)) {
            const drift::TimeUs maxOverlap =
                qMin(fromClip->timelineDuration, toClip->timelineDuration) - drift::secondsToUs(0.05);
            const drift::TimeUs clamped = qBound(drift::secondsToUs(0.1), durationUs, qMax(drift::secondsToUs(0.1), maxOverlap));
            toClip->timelineStart = fromClip->timelineEnd() - clamped;
            transition.durationUs = clamped;
        } else {
            transition.durationUs = durationUs;
        }

        pushProjectEdit(before, QStringLiteral("Transition duration"));
        finishEdit(QStringLiteral("Transition duration updated"));
        emit selectedTransitionDataChanged();
        return;
    }
}

void AppController::setTransitionKind(int trackIndex, const QString &transitionId, const QString &kind)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    const drift::Project before = m_project;
    for (drift::Transition &transition : track.transitions) {
        if (transition.id == transitionId) {
            transition.kind = drift::transitionKindFromString(kind);
            pushProjectEdit(before, QStringLiteral("Transition kind"));
            finishEdit(QStringLiteral("Transition kind updated"));
            emit selectedTransitionDataChanged();
            return;
        }
    }
}

QVariantMap AppController::transitionBetweenClips(int trackIndex, int clipIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return {};

    const drift::Track &track = m_project.tracks().at(trackIndex);
    const int partnerIndex = findTransitionPartnerIndex(track, clipIndex);
    if (partnerIndex < 0)
        return {};

    const QString fromId = track.clips.at(clipIndex).id;
    const QString toId = track.clips.at(partnerIndex).id;
    for (const drift::Transition &transition : track.transitions) {
        if (transition.fromClipId == fromId && transition.toClipId == toId)
            return transitionToMap(track, transition);
    }
    return {};
}

QVariantList AppController::transitionKinds() const
{
    const QList<drift::TransitionKind> kinds = {
        drift::TransitionKind::Crossfade,
        drift::TransitionKind::DipToBlack,
        drift::TransitionKind::DipToWhite,
        drift::TransitionKind::WipeLeft,
        drift::TransitionKind::WipeRight,
        drift::TransitionKind::WipeUp,
        drift::TransitionKind::WipeDown,
        drift::TransitionKind::PushLeft,
        drift::TransitionKind::ZoomIn,
    };

    QVariantList result;
    result.reserve(kinds.size());
    for (drift::TransitionKind kind : kinds) {
        result.append(QVariantMap{
            {QStringLiteral("kind"), drift::transitionKindToString(kind)},
            {QStringLiteral("label"), drift::transitionKindLabel(kind)},
        });
    }
    return result;
}

QVariantMap AppController::selectedTransitionData() const
{
    if (m_selectedTransitionTrack < 0 || m_selectedTransitionLeftClip < 0)
        return {};
    return transitionBetweenClips(m_selectedTransitionTrack, m_selectedTransitionLeftClip);
}

void AppController::selectTransition(int trackIndex, int leftClipIndex)
{
    if (transitionBetweenClips(trackIndex, leftClipIndex).isEmpty())
        return;

    m_selectedTransitionTrack = trackIndex;
    m_selectedTransitionLeftClip = leftClipIndex;
    m_selectedTrack = trackIndex;
    m_selectedClip = leftClipIndex;
    m_selection = {qMakePair(trackIndex, leftClipIndex)};
    emit selectionChanged();
    emit selectedTransitionDataChanged();
}

void AppController::clearTransitionSelection()
{
    if (m_selectedTransitionTrack < 0 && m_selectedTransitionLeftClip < 0)
        return;

    m_selectedTransitionTrack = -1;
    m_selectedTransitionLeftClip = -1;
    emit selectedTransitionDataChanged();
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

void AppController::resetClipTransform(int trackIndex, int clipIndex)
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return;

    drift::Track &track = m_project.tracks()[trackIndex];
    if (clipIndex < 0 || clipIndex >= track.clips.size())
        return;

    drift::Clip &clip = track.clips[clipIndex];
    if (clip.type == drift::ClipType::Audio)
        return;

    const drift::Project before = m_project;
    clip.opacity = {};
    clip.transformX = {};
    clip.transformY = {};
    clip.transformW = {};
    clip.transformH = {};
    clip.rotation = {};
    setClipLayoutPixels(clip, 0, 0, m_project.width(), m_project.height());
    pushProjectEdit(before, QStringLiteral("Reset transform"));
    finishEdit(QStringLiteral("Transform reset"));
}

QVariantList AppController::effectCatalog() const
{
    QVariantList out;
    for (const EffectPresetEntry &def : ::effectCatalog()) {
        QVariantList params;
        for (const drift::EffectParamSpec &p : def.meta.parameters) {
            params.append(QVariantMap{
                {QStringLiteral("key"), p.key},
                {QStringLiteral("label"), p.label},
                {QStringLiteral("min"), p.min},
                {QStringLiteral("max"), p.max},
                {QStringLiteral("default"), p.defaultValue},
                {QStringLiteral("isBoolean"), p.isBoolean},
            });
        }
        out.append(QVariantMap{
            {QStringLiteral("id"), def.meta.id},
            {QStringLiteral("label"), def.meta.displayName},
            {QStringLiteral("displayName"), def.meta.displayName},
            {QStringLiteral("category"), def.meta.category},
            {QStringLiteral("categoryLabel"), effectCategoryLabel(def.meta.category)},
            {QStringLiteral("compositorOnly"), def.meta.compositorOnly},
            {QStringLiteral("params"), params},
        });
    }
    return out;
}

QVariantList AppController::effectCategories() const
{
    QVariantList out;
    for (const auto &entry : ::effectCategories()) {
        out.append(QVariantMap{
            {QStringLiteral("id"), entry.first},
            {QStringLiteral("label"), entry.second},
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

    const EffectPresetEntry *def = effectDefForId(effectId);
    if (!def)
        return;

    drift::Effect effect;
    effect.name = def->filterName;
    effect.catalogId = def->meta.id;
    for (auto it = def->fixedParams.constBegin(); it != def->fixedParams.constEnd(); ++it)
        effect.parameters.insert(it.key(), it.value());
    for (const drift::EffectParamSpec &p : def->meta.parameters) {
        if (p.isBoolean)
            effect.parameters.insert(p.key, p.defaultValue > 0.5);
        else
            effect.parameters.insert(p.key, p.defaultValue);
    }

    const drift::Project before = m_project;
    track.clips[clipIndex].effects.append(effect);
    m_selectedTrack = trackIndex;
    m_selectedClip = clipIndex;
    m_selection = {qMakePair(trackIndex, clipIndex)};
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
    const EffectPresetEntry *def = effectDefForId(clip.effects[effectIndex].catalogId);
    bool asBoolean = false;
    if (def) {
        for (const drift::EffectParamSpec &param : def->meta.parameters) {
            if (param.key == key) {
                asBoolean = param.isBoolean;
                break;
            }
        }
    }
    if (asBoolean)
        clip.effects[effectIndex].parameters.insert(key, value > 0.5);
    else
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

    const drift::Project before = m_project;
    const int trackIndex = drift::ensureTrackForClipType(m_project, drift::ClipType::Image, false);

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

void AppController::copySelection()
{
    m_clipboard.clear();
    QList<QPair<int, int>> pairs = m_selection;
    if (pairs.isEmpty() && m_selectedTrack >= 0 && m_selectedClip >= 0)
        pairs.append(qMakePair(m_selectedTrack, m_selectedClip));
    for (const QPair<int, int> &pair : pairs) {
        if (!isValidClipIndex(pair.first, pair.second))
            continue;
        ClipboardItem item;
        item.clip = m_project.tracks().at(pair.first).clips.at(pair.second);
        item.trackType = m_project.tracks().at(pair.first).type;
        m_clipboard.append(item);
    }
    setLastMessage(QStringLiteral("Copied %1 clip(s)").arg(m_clipboard.size()));
}

void AppController::cutSelection()
{
    copySelection();
    deleteSelectedClip();
}

void AppController::pasteAtPlayhead()
{
    if (m_clipboard.isEmpty())
        return;
    const drift::Project before = m_project;
    drift::TimeUs anchor = LLONG_MAX;
    for (const ClipboardItem &item : m_clipboard)
        anchor = qMin(anchor, item.clip.timelineStart);
    const drift::TimeUs shift = m_playheadUs - anchor;
    QList<QPair<int, int>> inserted;

    for (const ClipboardItem &item : m_clipboard) {
        drift::Clip clip = item.clip;
        clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        clip.timelineStart = qMax<drift::TimeUs>(0, clip.timelineStart + shift);

        int targetTrack = -1;
        for (int i = 0; i < m_project.tracks().size(); ++i) {
            if (m_project.tracks().at(i).type == item.trackType && m_project.tracks().at(i).allowsClipType(clip.type)) {
                targetTrack = i;
                break;
            }
        }
        if (targetTrack < 0)
            targetTrack = drift::ensureTrackForClipType(m_project, clip.type, true);
        if (targetTrack < 0 || !m_project.tracks()[targetTrack].allowsClipType(clip.type))
            continue;
        drift::Track &track = m_project.tracks()[targetTrack];
        track.clips.append(clip);
        inserted.append(qMakePair(targetTrack, track.clips.size() - 1));
    }

    if (inserted.isEmpty())
        return;
    pushProjectEdit(before, QStringLiteral("Paste"));
    m_selection = inserted;
    m_selectedTrack = inserted.constLast().first;
    m_selectedClip = inserted.constLast().second;
    finishEdit(QStringLiteral("Pasted %1 clip(s)").arg(inserted.size()));
}

void AppController::nudgeSelection(double deltaSeconds)
{
    if (qFuzzyIsNull(deltaSeconds))
        return;
    QList<QPair<int, int>> pairs = m_selection;
    if (pairs.isEmpty() && m_selectedTrack >= 0 && m_selectedClip >= 0)
        pairs.append(qMakePair(m_selectedTrack, m_selectedClip));
    if (pairs.isEmpty())
        return;
    const drift::Project before = m_project;
    const drift::TimeUs deltaUs = drift::secondsToUs(deltaSeconds);
    for (const QPair<int, int> &pair : pairs) {
        if (!isValidClipIndex(pair.first, pair.second))
            continue;
        drift::Clip &clip = m_project.tracks()[pair.first].clips[pair.second];
        clip.timelineStart = qMax<drift::TimeUs>(0, clip.timelineStart + deltaUs);
    }
    pushProjectEdit(before, QStringLiteral("Nudge selection"));
    finishEdit(QStringLiteral("Selection nudged"));
}

bool AppController::selectionContains(int trackIndex, int clipIndex) const
{
    return m_selection.contains(qMakePair(trackIndex, clipIndex));
}

QString AppController::shortcutFor(const QString &actionId) const
{
    return m_shortcuts.value(actionId);
}

void AppController::setShortcut(const QString &actionId, const QString &keys)
{
    if (!m_shortcuts.contains(actionId))
        return;
    m_shortcuts[actionId] = keys;
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    settings.setValue(actionId, keys);
    settings.endGroup();
    emit shortcutsChanged();
}

void AppController::triggerAction(const QString &actionId)
{
    if (actionId == QStringLiteral("playPause"))
        togglePlayback();
    else if (actionId == QStringLiteral("delete"))
        deleteSelectedClip();
    else if (actionId == QStringLiteral("undo"))
        undo();
    else if (actionId == QStringLiteral("redo"))
        redo();
    else if (actionId == QStringLiteral("clearSelection"))
        clearSelection();
    else if (actionId == QStringLiteral("duplicate"))
        duplicateSelectedClip();
    else if (actionId == QStringLiteral("split"))
        splitAtPlayhead();
    else if (actionId == QStringLiteral("copy"))
        copySelection();
    else if (actionId == QStringLiteral("cut"))
        cutSelection();
    else if (actionId == QStringLiteral("paste"))
        pasteAtPlayhead();
    else if (actionId == QStringLiteral("nudgeLeft"))
        nudgeSelection(-0.1);
    else if (actionId == QStringLiteral("nudgeRight"))
        nudgeSelection(0.1);
    else if (actionId == QStringLiteral("toggleGuides"))
        setGuidesEnabled(!guidesEnabled());
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

bool AppController::isValidClipIndex(int trackIndex, int clipIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_project.tracks().size())
        return false;
    return clipIndex >= 0 && clipIndex < m_project.tracks().at(trackIndex).clips.size();
}

void AppController::normalizeSelection()
{
    const QList<QPair<int, int>> selection = m_selection;
    QList<QPair<int, int>> kept;
    kept.reserve(selection.size());
    for (const QPair<int, int> &pair : selection) {
        if (isValidClipIndex(pair.first, pair.second) && !kept.contains(pair))
            kept.append(pair);
    }
    m_selection = kept;
    if (m_selection.isEmpty()) {
        m_selectedTrack = -1;
        m_selectedClip = -1;
        return;
    }
    if (!isValidClipIndex(m_selectedTrack, m_selectedClip)) {
        m_selectedTrack = m_selection.constLast().first;
        m_selectedClip = m_selection.constLast().second;
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
    emit backgroundChanged();
    setLastMessage(QStringLiteral("Project loaded"));
}

QVariantList AppController::exportPresets() const
{
    QVariantList out;
    for (const ExportPreset &preset : Exporter::presets()) {
        out.append(QVariantMap{
            {QStringLiteral("id"), preset.id},
            {QStringLiteral("label"), preset.label},
        });
    }
    return out;
}

double AppController::exportProgress() const
{
    return m_exportProgress;
}

void AppController::cancelExport()
{
    if (m_exportInProgress)
        m_exportCancel.storeRelaxed(1);
}

void AppController::exportProject(const QUrl &outputUrl)
{
    exportWithPreset(outputUrl, QStringLiteral("source"));
}

void AppController::exportWithPreset(const QUrl &outputUrl, const QString &presetId)
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

    const ExportPreset *presetPtr = Exporter::presetById(presetId);
    const ExportPreset preset = presetPtr ? *presetPtr : Exporter::presets().first();

    // Stop playback so the decode pool isn't driven from two threads at once.
    setPlaying(false);

    m_exportCancel.storeRelaxed(0);
    m_exportProgress = 0.0;
    emit exportProgressChanged();
    m_exportInProgress = true;
    emit exportInProgressChanged();
    setLastMessage(QStringLiteral("Exporting..."));

    // Snapshot the project so edits during export can't race the encoder.
    const drift::Project snapshot = m_project;

    (void)QtConcurrent::run([this, snapshot, preset, outputPath]() {
        QString error;
        const bool ok = Exporter::run(
            snapshot, preset, outputPath, &error, [this](double fraction) {
                QMetaObject::invokeMethod(
                    this,
                    [this, fraction]() {
                        m_exportProgress = fraction;
                        emit exportProgressChanged();
                    },
                    Qt::QueuedConnection);
                return m_exportCancel.loadRelaxed() == 0;
            });

        QMetaObject::invokeMethod(
            this,
            [this, ok, error]() {
                m_exportInProgress = false;
                m_exportProgress = ok ? 1.0 : 0.0;
                emit exportProgressChanged();
                emit exportInProgressChanged();
                setLastMessage(ok ? QStringLiteral("Export complete") : error);
                emit exportFinished(ok);
            },
            Qt::QueuedConnection);
    });
}
