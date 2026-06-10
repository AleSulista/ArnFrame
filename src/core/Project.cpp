#include "Project.h"

#include "Clip.h"
#include "SubtitleCue.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <QtMath>

namespace drift {

namespace {

QJsonObject keyframesToJson(const KeyframeTrack<double> &track)
{
    QJsonArray keyframes;
    for (auto it = track.keyframes().constBegin(); it != track.keyframes().constEnd(); ++it) {
        keyframes.append(QJsonObject{
            {QStringLiteral("timeUs"), static_cast<double>(it.key())},
            {QStringLiteral("value"), it.value()},
        });
    }
    return QJsonObject{
        {QStringLiteral("interpolation"), interpolationToString(track.interpolation())},
        {QStringLiteral("keyframes"), keyframes},
    };
}

KeyframeTrack<double> keyframesFromJson(const QJsonObject &object)
{
    KeyframeTrack<double> track;
    track.setInterpolation(
        interpolationFromString(object.value(QStringLiteral("interpolation")).toString()));

    for (const QJsonValue &value : object.value(QStringLiteral("keyframes")).toArray()) {
        const QJsonObject keyframe = value.toObject();
        track.setKeyframe(static_cast<TimeUs>(keyframe.value(QStringLiteral("timeUs")).toDouble()),
                          keyframe.value(QStringLiteral("value")).toDouble(1.0));
    }
    return track;
}

QJsonArray effectsToJson(const QList<Effect> &effects)
{
    QJsonArray array;
    for (const Effect &effect : effects) {
        QJsonObject params;
        for (auto it = effect.parameters.constBegin(); it != effect.parameters.constEnd(); ++it)
            params.insert(it.key(), QJsonValue::fromVariant(it.value()));
        array.append(QJsonObject{
            {QStringLiteral("name"), effect.name},
            {QStringLiteral("catalogId"), effect.catalogId},
            {QStringLiteral("parameters"), params},
        });
    }
    return array;
}

QList<Effect> effectsFromJson(const QJsonArray &array)
{
    QList<Effect> effects;
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        Effect effect;
        effect.name = object.value(QStringLiteral("name")).toString();
        effect.catalogId = object.value(QStringLiteral("catalogId")).toString();
        const QJsonObject params = object.value(QStringLiteral("parameters")).toObject();
        for (auto it = params.constBegin(); it != params.constEnd(); ++it)
            effect.parameters.insert(it.key(), it.value().toVariant());
        effects.append(effect);
    }
    return effects;
}

QJsonObject textStyleToJson(const TextStyle &s)
{
    return QJsonObject{
        {QStringLiteral("fontFamily"), s.fontFamily},
        {QStringLiteral("pixelSize"), s.pixelSize},
        {QStringLiteral("fontWeight"), s.fontWeight},
        {QStringLiteral("italic"), s.italic},
        {QStringLiteral("color"), s.color.name(QColor::HexArgb)},
        {QStringLiteral("align"), textAlignToString(s.align)},
        {QStringLiteral("valign"), textVAlignToString(s.valign)},
        {QStringLiteral("wordWrap"), s.wordWrap},
        {QStringLiteral("lineHeight"), s.lineHeight},
        {QStringLiteral("letterSpacing"), s.letterSpacing},
        {QStringLiteral("outlineWidth"), s.outlineWidth},
        {QStringLiteral("outlineColor"), s.outlineColor.name(QColor::HexArgb)},
        {QStringLiteral("shadowEnabled"), s.shadowEnabled},
        {QStringLiteral("shadowOffsetX"), s.shadowOffsetX},
        {QStringLiteral("shadowOffsetY"), s.shadowOffsetY},
        {QStringLiteral("shadowBlur"), s.shadowBlur},
        {QStringLiteral("shadowOpacity"), s.shadowOpacity},
        {QStringLiteral("shadowColor"), s.shadowColor.name(QColor::HexArgb)},
        {QStringLiteral("boxEnabled"), s.boxEnabled},
        {QStringLiteral("boxColor"), s.boxColor.name(QColor::HexArgb)},
        {QStringLiteral("boxPadding"), s.boxPadding},
        {QStringLiteral("boxRadius"), s.boxRadius},
        {QStringLiteral("animInKind"), textAnimKindToString(s.animIn.kind)},
        {QStringLiteral("animInDurationUs"), static_cast<qint64>(s.animIn.durationUs)},
        {QStringLiteral("animInEase"), textEaseToString(s.animIn.ease)},
        {QStringLiteral("animOutKind"), textAnimKindToString(s.animOut.kind)},
        {QStringLiteral("animOutDurationUs"), static_cast<qint64>(s.animOut.durationUs)},
        {QStringLiteral("animOutEase"), textEaseToString(s.animOut.ease)},
    };
}

TextStyle textStyleFromJson(const QJsonObject &o)
{
    TextStyle s;
    if (o.isEmpty())
        return s; // old projects: keep defaults
    s.fontFamily = o.value(QStringLiteral("fontFamily")).toString(s.fontFamily);
    s.pixelSize = o.value(QStringLiteral("pixelSize")).toInt(s.pixelSize);
    // Projects written before the weight ladder only had a bold flag.
    if (o.contains(QStringLiteral("fontWeight")))
        s.fontWeight = qBound(100, o.value(QStringLiteral("fontWeight")).toInt(s.fontWeight), 900);
    else
        s.fontWeight = o.value(QStringLiteral("bold")).toBool(true) ? 700 : 400;
    s.italic = o.value(QStringLiteral("italic")).toBool(s.italic);
    s.color = QColor(o.value(QStringLiteral("color")).toString(s.color.name(QColor::HexArgb)));
    s.align = textAlignFromString(o.value(QStringLiteral("align")).toString());
    s.valign = textVAlignFromString(o.value(QStringLiteral("valign")).toString());
    s.wordWrap = o.value(QStringLiteral("wordWrap")).toBool(s.wordWrap);
    s.lineHeight = o.value(QStringLiteral("lineHeight")).toDouble(s.lineHeight);
    s.letterSpacing = o.value(QStringLiteral("letterSpacing")).toDouble(s.letterSpacing);
    s.outlineWidth = o.value(QStringLiteral("outlineWidth")).toDouble(s.outlineWidth);
    s.outlineColor = QColor(o.value(QStringLiteral("outlineColor")).toString(s.outlineColor.name(QColor::HexArgb)));
    s.shadowEnabled = o.value(QStringLiteral("shadowEnabled")).toBool(s.shadowEnabled);
    s.shadowOffsetX = o.value(QStringLiteral("shadowOffsetX")).toDouble(s.shadowOffsetX);
    s.shadowOffsetY = o.value(QStringLiteral("shadowOffsetY")).toDouble(s.shadowOffsetY);
    s.shadowBlur = o.value(QStringLiteral("shadowBlur")).toDouble(s.shadowBlur);
    s.shadowOpacity = o.value(QStringLiteral("shadowOpacity")).toDouble(s.shadowOpacity);
    s.shadowColor = QColor(o.value(QStringLiteral("shadowColor")).toString(s.shadowColor.name(QColor::HexArgb)));
    s.boxEnabled = o.value(QStringLiteral("boxEnabled")).toBool(s.boxEnabled);
    s.boxColor = QColor(o.value(QStringLiteral("boxColor")).toString(s.boxColor.name(QColor::HexArgb)));
    s.boxPadding = o.value(QStringLiteral("boxPadding")).toDouble(s.boxPadding);
    s.boxRadius = o.value(QStringLiteral("boxRadius")).toDouble(s.boxRadius);
    s.animIn.kind = textAnimKindFromString(o.value(QStringLiteral("animInKind")).toString());
    s.animIn.durationUs = o.value(QStringLiteral("animInDurationUs")).toInteger(s.animIn.durationUs);
    s.animIn.ease = textEaseFromString(o.value(QStringLiteral("animInEase")).toString());
    s.animOut.kind = textAnimKindFromString(o.value(QStringLiteral("animOutKind")).toString());
    s.animOut.durationUs = o.value(QStringLiteral("animOutDurationUs")).toInteger(s.animOut.durationUs);
    s.animOut.ease = textEaseFromString(o.value(QStringLiteral("animOutEase")).toString());
    return s;
}

QJsonObject shapeStyleToJson(const ShapeStyle &s)
{
    return QJsonObject{
        {QStringLiteral("kind"), shapeKindToString(s.kind)},
        {QStringLiteral("fill"), s.fill.name(QColor::HexArgb)},
        {QStringLiteral("stroke"), s.stroke.name(QColor::HexArgb)},
        {QStringLiteral("strokeWidth"), s.strokeWidth},
    };
}

ShapeStyle shapeStyleFromJson(const QJsonObject &o)
{
    ShapeStyle s;
    if (o.isEmpty())
        return s;
    s.kind = shapeKindFromString(o.value(QStringLiteral("kind")).toString());
    s.fill = QColor(o.value(QStringLiteral("fill")).toString(s.fill.name(QColor::HexArgb)));
    s.stroke = QColor(o.value(QStringLiteral("stroke")).toString(s.stroke.name(QColor::HexArgb)));
    s.strokeWidth = o.value(QStringLiteral("strokeWidth")).toDouble(s.strokeWidth);
    return s;
}

QJsonObject maskToJson(const Mask &m)
{
    QJsonArray points;
    for (const QPointF &pt : m.points)
        points.append(QJsonArray{pt.x(), pt.y()});

    return QJsonObject{
        {QStringLiteral("shape"), maskShapeToString(m.shape)},
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

Mask maskFromJson(const QJsonObject &o)
{
    Mask m;
    if (o.isEmpty())
        return m;
    m.shape = maskShapeFromString(o.value(QStringLiteral("shape")).toString());
    m.x = o.value(QStringLiteral("x")).toDouble(m.x);
    m.y = o.value(QStringLiteral("y")).toDouble(m.y);
    m.w = o.value(QStringLiteral("w")).toDouble(m.w);
    m.h = o.value(QStringLiteral("h")).toDouble(m.h);
    m.rotation = o.value(QStringLiteral("rotation")).toDouble(m.rotation);
    m.feather = o.value(QStringLiteral("feather")).toDouble(m.feather);
    m.invert = o.value(QStringLiteral("invert")).toBool(m.invert);
    const QJsonArray points = o.value(QStringLiteral("points")).toArray();
    for (const QJsonValue &value : points) {
        const QJsonArray pair = value.toArray();
        if (pair.size() >= 2)
            m.points.append(QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
    }
    return m;
}

QJsonObject transitionToJson(const Transition &t)
{
    QJsonObject params;
    for (auto it = t.parameters.constBegin(); it != t.parameters.constEnd(); ++it)
        params.insert(it.key(), QJsonValue::fromVariant(it.value()));

    // "kind" holds the transition package id. The pre-shader enum serialized the same strings,
    // so projects written by older builds keep loading.
    return QJsonObject{
        {QStringLiteral("id"), t.id},
        {QStringLiteral("fromClipId"), t.fromClipId},
        {QStringLiteral("toClipId"), t.toClipId},
        {QStringLiteral("kind"), t.kindId},
        {QStringLiteral("parameters"), params},
        {QStringLiteral("durationUs"), static_cast<double>(t.durationUs)},
    };
}

Transition transitionFromJson(const QJsonObject &o)
{
    Transition t;
    if (o.isEmpty())
        return t;
    t.id = o.value(QStringLiteral("id")).toString();
    t.fromClipId = o.value(QStringLiteral("fromClipId")).toString();
    t.toClipId = o.value(QStringLiteral("toClipId")).toString();
    const QString kind = o.value(QStringLiteral("kind")).toString();
    if (!kind.isEmpty())
        t.kindId = kind;
    const QJsonObject params = o.value(QStringLiteral("parameters")).toObject();
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        t.parameters.insert(it.key(), it.value().toVariant());
    t.durationUs = static_cast<TimeUs>(o.value(QStringLiteral("durationUs")).toDouble(t.durationUs));
    return t;
}

QJsonObject backgroundToJson(const Background &bg)
{
    return QJsonObject{
        {QStringLiteral("kind"),
         bg.kind == BackgroundKind::Blur ? QStringLiteral("blur") : QStringLiteral("color")},
        {QStringLiteral("color"), bg.color.name(QColor::HexArgb)},
        {QStringLiteral("blurStrength"), bg.blurStrength},
    };
}

Background backgroundFromJson(const QJsonObject &o)
{
    Background bg;
    if (o.isEmpty())
        return bg; // old projects: default solid black
    bg.kind = o.value(QStringLiteral("kind")).toString() == QStringLiteral("blur")
                  ? BackgroundKind::Blur
                  : BackgroundKind::Color;
    bg.color = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#ff000000")));
    bg.blurStrength = o.value(QStringLiteral("blurStrength")).toDouble(bg.blurStrength);
    return bg;
}

QJsonArray subtitleCuesToJson(const QList<SubtitleCue> &cues)
{
    QJsonArray array;
    for (const SubtitleCue &cue : cues) {
        array.append(QJsonObject{
            {QStringLiteral("startUs"), static_cast<double>(cue.startUs)},
            {QStringLiteral("endUs"), static_cast<double>(cue.endUs)},
            {QStringLiteral("text"), cue.text},
        });
    }
    return array;
}

QList<SubtitleCue> subtitleCuesFromJson(const QJsonArray &array)
{
    QList<SubtitleCue> cues;
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        SubtitleCue cue;
        cue.startUs = static_cast<TimeUs>(object.value(QStringLiteral("startUs")).toDouble());
        cue.endUs = static_cast<TimeUs>(object.value(QStringLiteral("endUs")).toDouble());
        cue.text = object.value(QStringLiteral("text")).toString();
        cues.append(cue);
    }
    sortSubtitleCues(cues);
    return cues;
}

QJsonObject clipToJson(const Clip &clip)
{
    return QJsonObject{
        {QStringLiteral("id"), clip.id},
        {QStringLiteral("assetId"), clip.assetId},
        {QStringLiteral("type"), clipTypeToString(clip.type)},
        {QStringLiteral("name"), clip.name},
        {QStringLiteral("textContent"), clip.textContent},
        {QStringLiteral("textStyle"), textStyleToJson(clip.textStyle)},
        {QStringLiteral("subtitleCues"), subtitleCuesToJson(clip.subtitleCues)},
        {QStringLiteral("shapeStyle"), shapeStyleToJson(clip.shapeStyle)},
        {QStringLiteral("path"), clip.path},
        {QStringLiteral("thumbnailPath"), clip.thumbnailPath},
        {QStringLiteral("filmstripPath"), clip.filmstripPath},
        {QStringLiteral("blendMode"), blendModeToString(clip.blendMode)},
        {QStringLiteral("speed"), clip.speed},
        {QStringLiteral("reverse"), clip.reverse},
        {QStringLiteral("flipH"), clip.flipH},
        {QStringLiteral("flipV"), clip.flipV},
        {QStringLiteral("mask"), maskToJson(clip.mask)},
        {QStringLiteral("fadeInUs"), static_cast<double>(clip.fadeInUs)},
        {QStringLiteral("fadeOutUs"), static_cast<double>(clip.fadeOutUs)},
        {QStringLiteral("fadeCurve"), fadeCurveToString(clip.fadeCurve)},
        {QStringLiteral("timelineStartUs"), static_cast<double>(clip.timelineStart)},
        {QStringLiteral("timelineDurationUs"), static_cast<double>(clip.timelineDuration)},
        {QStringLiteral("srcInUs"), static_cast<double>(clip.srcIn)},
        {QStringLiteral("srcOutUs"), static_cast<double>(clip.srcOut)},
        {QStringLiteral("volume"), keyframesToJson(clip.volume)},
        {QStringLiteral("opacity"), keyframesToJson(clip.opacity)},
        {QStringLiteral("x"), keyframesToJson(clip.transformX)},
        {QStringLiteral("y"), keyframesToJson(clip.transformY)},
        {QStringLiteral("width"), keyframesToJson(clip.transformW)},
        {QStringLiteral("height"), keyframesToJson(clip.transformH)},
        {QStringLiteral("rotation"), keyframesToJson(clip.rotation)},
        {QStringLiteral("effects"), effectsToJson(clip.effects)},
    };
}

KeyframeTrack<double> singleKeyframe(double value)
{
    KeyframeTrack<double> track;
    track.setKeyframe(0, value);
    return track;
}

void applyLegacyFractionalLayout(Clip &clip, const KeyframeTrack<double> &posX,
                                 const KeyframeTrack<double> &posY, const KeyframeTrack<double> &scale,
                                 int canvasW, int canvasH)
{
    const double cx = (posX.isEmpty() ? 0.5 : posX.evaluateAt(0)) * canvasW;
    const double cy = (posY.isEmpty() ? 0.5 : posY.evaluateAt(0)) * canvasH;
    const double s = scale.isEmpty() ? 1.0 : scale.evaluateAt(0);
    const double w = qMax(1.0, canvasW * s);
    const double h = qMax(1.0, canvasH * s);
    clip.transformX = singleKeyframe(cx - w * 0.5);
    clip.transformY = singleKeyframe(cy - h * 0.5);
    clip.transformW = singleKeyframe(w);
    clip.transformH = singleKeyframe(h);

    // Preserve interpolation mode from the primary legacy track when present.
    if (!posX.isEmpty()) {
        clip.transformX.setInterpolation(posX.interpolation());
        clip.transformY.setInterpolation(posY.isEmpty() ? posX.interpolation() : posY.interpolation());
        clip.transformW.setInterpolation(scale.isEmpty() ? posX.interpolation() : scale.interpolation());
        clip.transformH.setInterpolation(scale.isEmpty() ? posX.interpolation() : scale.interpolation());
    }
}

Clip clipFromJsonV2(const QJsonObject &object, int canvasW = 1920, int canvasH = 1080)
{
    Clip clip;
    clip.id = object.value(QStringLiteral("id")).toString(QUuid::createUuid().toString(QUuid::WithoutBraces));
    clip.assetId = object.value(QStringLiteral("assetId")).toString();
    clip.type = clipTypeFromString(object.value(QStringLiteral("type")).toString());
    clip.name = object.value(QStringLiteral("name")).toString();
    clip.textContent = object.value(QStringLiteral("textContent")).toString();
    clip.textStyle = textStyleFromJson(object.value(QStringLiteral("textStyle")).toObject());
    clip.subtitleCues = subtitleCuesFromJson(object.value(QStringLiteral("subtitleCues")).toArray());
    clip.shapeStyle = shapeStyleFromJson(object.value(QStringLiteral("shapeStyle")).toObject());
    clip.path = object.value(QStringLiteral("path")).toString();
    clip.thumbnailPath = object.value(QStringLiteral("thumbnailPath")).toString();
    clip.filmstripPath = object.value(QStringLiteral("filmstripPath")).toString();
    clip.blendMode = blendModeFromString(object.value(QStringLiteral("blendMode")).toString());
    clip.speed = object.value(QStringLiteral("speed")).toDouble(1.0);
    clip.reverse = object.value(QStringLiteral("reverse")).toBool(false);
    clip.flipH = object.value(QStringLiteral("flipH")).toBool(false);
    clip.flipV = object.value(QStringLiteral("flipV")).toBool(false);
    clip.mask = maskFromJson(object.value(QStringLiteral("mask")).toObject());
    clip.fadeInUs = static_cast<TimeUs>(object.value(QStringLiteral("fadeInUs")).toDouble());
    clip.fadeOutUs = static_cast<TimeUs>(object.value(QStringLiteral("fadeOutUs")).toDouble());
    clip.fadeCurve = fadeCurveFromString(object.value(QStringLiteral("fadeCurve")).toString());
    clip.timelineStart = static_cast<TimeUs>(object.value(QStringLiteral("timelineStartUs")).toDouble());
    clip.timelineDuration = static_cast<TimeUs>(object.value(QStringLiteral("timelineDurationUs")).toDouble());
    clip.srcIn = static_cast<TimeUs>(object.value(QStringLiteral("srcInUs")).toDouble());
    clip.srcOut = static_cast<TimeUs>(object.value(QStringLiteral("srcOutUs")).toDouble());
    if (object.value(QStringLiteral("volume")).isObject()) {
        clip.volume = keyframesFromJson(object.value(QStringLiteral("volume")).toObject());
    } else {
        clip.volume.setKeyframe(0, object.value(QStringLiteral("volume")).toDouble(1.0));
    }
    clip.opacity = keyframesFromJson(object.value(QStringLiteral("opacity")).toObject());
    clip.rotation = keyframesFromJson(object.value(QStringLiteral("rotation")).toObject());
    clip.effects = effectsFromJson(object.value(QStringLiteral("effects")).toArray());

    const bool hasPixelLayout = object.value(QStringLiteral("x")).isObject()
                                || object.value(QStringLiteral("width")).isObject();
    if (hasPixelLayout) {
        clip.transformX = keyframesFromJson(object.value(QStringLiteral("x")).toObject());
        clip.transformY = keyframesFromJson(object.value(QStringLiteral("y")).toObject());
        clip.transformW = keyframesFromJson(object.value(QStringLiteral("width")).toObject());
        clip.transformH = keyframesFromJson(object.value(QStringLiteral("height")).toObject());
    } else {
        applyLegacyFractionalLayout(clip,
                                    keyframesFromJson(object.value(QStringLiteral("posX")).toObject()),
                                    keyframesFromJson(object.value(QStringLiteral("posY")).toObject()),
                                    keyframesFromJson(object.value(QStringLiteral("scale")).toObject()),
                                    qMax(1, canvasW), qMax(1, canvasH));
    }
    return clip;
}

Clip clipFromJsonV1(const QJsonObject &object, const QList<QString> &assetOrder)
{
    Clip clip;
    clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clip.name = object.value(QStringLiteral("name")).toString();
    clip.path = object.value(QStringLiteral("path")).toString();
    clip.type = clipTypeFromString(object.value(QStringLiteral("kind")).toString());
    clip.textContent = object.value(QStringLiteral("textContent")).toString();
    clip.thumbnailPath = object.value(QStringLiteral("thumbnailPath")).toString();
    clip.filmstripPath = object.value(QStringLiteral("filmstripPath")).toString();
    clip.timelineStart = secondsToUs(object.value(QStringLiteral("start")).toDouble());
    clip.timelineDuration = secondsToUs(object.value(QStringLiteral("duration")).toDouble());
    clip.srcIn = secondsToUs(object.value(QStringLiteral("inPoint")).toDouble());
    clip.srcOut = secondsToUs(object.value(QStringLiteral("outPoint")).toDouble());

    const int assetIndex = object.value(QStringLiteral("assetIndex")).toInt(-1);
    if (assetIndex >= 0 && assetIndex < assetOrder.size())
        clip.assetId = assetOrder.at(assetIndex);

    return clip;
}

QJsonObject assetToJson(const MediaAsset &asset)
{
    return QJsonObject{
        {QStringLiteral("id"), asset.id},
        {QStringLiteral("name"), asset.name},
        {QStringLiteral("kind"), mediaKindToString(asset.kind)},
        {QStringLiteral("durationUs"), static_cast<double>(asset.durationUs)},
        {QStringLiteral("duration"), asset.durationLabel},
        {QStringLiteral("path"), asset.path},
        {QStringLiteral("width"), asset.width},
        {QStringLiteral("height"), asset.height},
        {QStringLiteral("fps"), asset.fps},
        {QStringLiteral("rotationDegrees"), asset.rotationDegrees},
        {QStringLiteral("sampleRate"), asset.sampleRate},
        {QStringLiteral("channels"), asset.channels},
        {QStringLiteral("codecName"), asset.codecName},
        {QStringLiteral("thumbnailPath"), asset.thumbnailPath},
        {QStringLiteral("filmstripPath"), asset.filmstripPath},
    };
}

MediaAsset assetFromJsonV2(const QJsonObject &object)
{
    MediaAsset asset;
    asset.id = object.value(QStringLiteral("id")).toString(QUuid::createUuid().toString(QUuid::WithoutBraces));
    asset.name = object.value(QStringLiteral("name")).toString();
    asset.kind = mediaKindFromString(object.value(QStringLiteral("kind")).toString());
    asset.durationUs = static_cast<TimeUs>(object.value(QStringLiteral("durationUs")).toDouble());
    asset.durationLabel = object.value(QStringLiteral("duration")).toString();
    asset.path = object.value(QStringLiteral("path")).toString();
    asset.width = object.value(QStringLiteral("width")).toInt();
    asset.height = object.value(QStringLiteral("height")).toInt();
    asset.fps = object.value(QStringLiteral("fps")).toDouble();
    asset.rotationDegrees = object.value(QStringLiteral("rotationDegrees")).toInt();
    asset.sampleRate = object.value(QStringLiteral("sampleRate")).toInt();
    asset.channels = object.value(QStringLiteral("channels")).toInt();
    asset.codecName = object.value(QStringLiteral("codecName")).toString();
    asset.thumbnailPath = object.value(QStringLiteral("thumbnailPath")).toString();
    asset.filmstripPath = object.value(QStringLiteral("filmstripPath")).toString();
    return asset;
}

MediaAsset assetFromJsonV1(const QJsonObject &object)
{
    MediaAsset asset;
    asset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    asset.name = object.value(QStringLiteral("name")).toString();
    asset.kind = mediaKindFromString(object.value(QStringLiteral("kind")).toString());
    asset.durationLabel = object.value(QStringLiteral("duration")).toString();
    asset.durationUs = secondsToUs(object.value(QStringLiteral("durationSeconds")).toDouble());
    asset.path = object.value(QStringLiteral("path")).toString();
    asset.thumbnailPath = object.value(QStringLiteral("thumbnailPath")).toString();
    asset.filmstripPath = object.value(QStringLiteral("filmstripPath")).toString();
    return asset;
}

} // namespace

void Project::resetToDefaultTimeline()
{
    m_tracks = {
        {.type = TrackType::Video},
    };
}

TimeUs Project::durationUs() const
{
    TimeUs maxEnd = 0;
    for (const Track &track : m_tracks) {
        for (const Clip &clip : track.clips)
            maxEnd = qMax(maxEnd, clip.timelineEnd());
    }
    return maxEnd;
}

QString Project::addAsset(MediaAsset asset)
{
    if (asset.id.isEmpty())
        asset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    m_assetsById.insert(asset.id, asset);
    if (!m_assetOrder.contains(asset.id))
        m_assetOrder.append(asset.id);
    return asset.id;
}

MediaAsset *Project::asset(const QString &id)
{
    auto it = m_assetsById.find(id);
    return it == m_assetsById.end() ? nullptr : &it.value();
}

const MediaAsset *Project::asset(const QString &id) const
{
    auto it = m_assetsById.constFind(id);
    return it == m_assetsById.constEnd() ? nullptr : &it.value();
}

int Project::assetIndex(const QString &id) const
{
    return m_assetOrder.indexOf(id);
}

QString Project::assetIdAt(int index) const
{
    if (index < 0 || index >= m_assetOrder.size())
        return {};
    return m_assetOrder.at(index);
}

Project Project::fromJson(const QJsonObject &object, QString *errorOut)
{
    Project project;
    const int version = object.value(QStringLiteral("version")).toInt(1);

    project.setName(object.value(QStringLiteral("projectName")).toString(QStringLiteral("Untitled Project")));
    project.setFps(object.value(QStringLiteral("fps")).toInt(30));
    project.setResolution(object.value(QStringLiteral("width")).toInt(1920),
                          object.value(QStringLiteral("height")).toInt(1080));
    project.setSampleRate(object.value(QStringLiteral("sampleRate")).toInt(48000));
    project.setBackground(backgroundFromJson(object.value(QStringLiteral("background")).toObject()));

    const QJsonArray assetsArray = object.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &value : assetsArray) {
        const QJsonObject assetObject = value.toObject();
        if (version >= 2)
            project.addAsset(assetFromJsonV2(assetObject));
        else
            project.addAsset(assetFromJsonV1(assetObject));
    }

    // Rebuild tracks from JSON. The Project ctor seeds a 1-track default, so
    // clearing first is required — otherwise only the first saved track loads.
    project.m_tracks.clear();
    const QJsonArray tracksArray = object.value(QStringLiteral("tracks")).toArray();
    if (tracksArray.isEmpty()) {
        project.resetToDefaultTimeline();
    } else {
        project.m_tracks.reserve(tracksArray.size());
        for (const QJsonValue &value : tracksArray) {
            const QJsonObject trackObject = value.toObject();
            Track track;
            track.type = trackTypeFromString(
                trackObject.value(QStringLiteral("type")).toString(QStringLiteral("video")));
            track.muted = trackObject.value(QStringLiteral("muted")).toBool(false);
            track.hidden = trackObject.value(QStringLiteral("hidden")).toBool(false);
            track.locked = trackObject.value(QStringLiteral("locked")).toBool(false);
            track.showWaveform = trackObject.value(QStringLiteral("showWaveform")).toBool(false);

            const QJsonArray clipsArray = trackObject.value(QStringLiteral("clips")).toArray();
            for (const QJsonValue &clipValue : clipsArray) {
                const QJsonObject clipObject = clipValue.toObject();
                if (version >= 2)
                    track.clips.append(clipFromJsonV2(clipObject, project.width(), project.height()));
                else
                    track.clips.append(clipFromJsonV1(clipObject, project.m_assetOrder));
            }

            const QJsonArray transitionsArray = trackObject.value(QStringLiteral("transitions")).toArray();
            for (const QJsonValue &transitionValue : transitionsArray)
                track.transitions.append(transitionFromJson(transitionValue.toObject()));

            project.m_tracks.append(track);
        }
    }

    project.m_bookmarks.clear();
    const QJsonArray bookmarksArray = object.value(QStringLiteral("bookmarks")).toArray();
    for (const QJsonValue &value : bookmarksArray) {
        const QJsonObject bookmarkObject = value.toObject();
        Bookmark bookmark;
        if (version >= 2) {
            bookmark.timeUs = static_cast<TimeUs>(bookmarkObject.value(QStringLiteral("timeUs")).toDouble());
        } else {
            bookmark.timeUs = secondsToUs(bookmarkObject.value(QStringLiteral("seconds")).toDouble());
        }
        bookmark.label = bookmarkObject.value(QStringLiteral("label")).toString();
        project.m_bookmarks.append(bookmark);
    }

    if (errorOut)
        errorOut->clear();
    return project;
}

QJsonObject Project::toJson() const
{
    QJsonArray assetsArray;
    for (const QString &id : m_assetOrder) {
        const MediaAsset *assetPtr = asset(id);
        if (assetPtr)
            assetsArray.append(assetToJson(*assetPtr));
    }

    QJsonArray tracksArray;
    for (const Track &track : m_tracks) {
        QJsonArray clipsArray;
        for (const Clip &clip : track.clips)
            clipsArray.append(clipToJson(clip));

        QJsonArray transitionsArray;
        for (const Transition &transition : track.transitions)
            transitionsArray.append(transitionToJson(transition));

        tracksArray.append(QJsonObject{
            {QStringLiteral("type"), trackTypeToString(track.type)},
            {QStringLiteral("muted"), track.muted},
            {QStringLiteral("hidden"), track.hidden},
            {QStringLiteral("locked"), track.locked},
            {QStringLiteral("showWaveform"), track.showWaveform},
            {QStringLiteral("clips"), clipsArray},
            {QStringLiteral("transitions"), transitionsArray},
        });
    }

    QJsonArray bookmarksArray;
    for (const Bookmark &bookmark : m_bookmarks) {
        bookmarksArray.append(QJsonObject{
            {QStringLiteral("timeUs"), static_cast<double>(bookmark.timeUs)},
            {QStringLiteral("label"), bookmark.label},
        });
    }

    return QJsonObject{
        {QStringLiteral("version"), kCurrentVersion},
        {QStringLiteral("projectName"), m_name},
        {QStringLiteral("fps"), m_fps},
        {QStringLiteral("width"), m_width},
        {QStringLiteral("height"), m_height},
        {QStringLiteral("sampleRate"), m_sampleRate},
        {QStringLiteral("assets"), assetsArray},
        {QStringLiteral("tracks"), tracksArray},
        {QStringLiteral("bookmarks"), bookmarksArray},
        {QStringLiteral("background"), backgroundToJson(m_background)},
    };
}

} // namespace drift
