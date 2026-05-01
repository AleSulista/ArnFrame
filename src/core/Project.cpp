#include "Project.h"

#include "Clip.h"

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
        {QStringLiteral("interpolation"),
         track.interpolation() == Interpolation::Hold ? QStringLiteral("hold")
                                                      : QStringLiteral("linear")},
        {QStringLiteral("keyframes"), keyframes},
    };
}

KeyframeTrack<double> keyframesFromJson(const QJsonObject &object)
{
    KeyframeTrack<double> track;
    const QString mode = object.value(QStringLiteral("interpolation")).toString();
    track.setInterpolation(mode == QStringLiteral("hold") ? Interpolation::Hold
                                                          : Interpolation::Linear);

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
        {QStringLiteral("color"), s.color.name(QColor::HexArgb)},
        {QStringLiteral("bold"), s.bold},
        {QStringLiteral("italic"), s.italic},
        {QStringLiteral("align"), textAlignToString(s.align)},
        {QStringLiteral("outlineWidth"), s.outlineWidth},
        {QStringLiteral("outlineColor"), s.outlineColor.name(QColor::HexArgb)},
        {QStringLiteral("boxEnabled"), s.boxEnabled},
        {QStringLiteral("boxColor"), s.boxColor.name(QColor::HexArgb)},
        {QStringLiteral("boxPadding"), s.boxPadding},
    };
}

TextStyle textStyleFromJson(const QJsonObject &o)
{
    TextStyle s;
    if (o.isEmpty())
        return s; // old projects: keep defaults
    s.fontFamily = o.value(QStringLiteral("fontFamily")).toString(s.fontFamily);
    s.pixelSize = o.value(QStringLiteral("pixelSize")).toInt(s.pixelSize);
    s.color = QColor(o.value(QStringLiteral("color")).toString(s.color.name(QColor::HexArgb)));
    s.bold = o.value(QStringLiteral("bold")).toBool(s.bold);
    s.italic = o.value(QStringLiteral("italic")).toBool(s.italic);
    s.align = textAlignFromString(o.value(QStringLiteral("align")).toString());
    s.outlineWidth = o.value(QStringLiteral("outlineWidth")).toDouble(s.outlineWidth);
    s.outlineColor = QColor(o.value(QStringLiteral("outlineColor")).toString(s.outlineColor.name(QColor::HexArgb)));
    s.boxEnabled = o.value(QStringLiteral("boxEnabled")).toBool(s.boxEnabled);
    s.boxColor = QColor(o.value(QStringLiteral("boxColor")).toString(s.boxColor.name(QColor::HexArgb)));
    s.boxPadding = o.value(QStringLiteral("boxPadding")).toDouble(s.boxPadding);
    return s;
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
        {QStringLiteral("path"), clip.path},
        {QStringLiteral("thumbnailPath"), clip.thumbnailPath},
        {QStringLiteral("filmstripPath"), clip.filmstripPath},
        {QStringLiteral("blendMode"), blendModeToString(clip.blendMode)},
        {QStringLiteral("timelineStartUs"), static_cast<double>(clip.timelineStart)},
        {QStringLiteral("timelineDurationUs"), static_cast<double>(clip.timelineDuration)},
        {QStringLiteral("srcInUs"), static_cast<double>(clip.srcIn)},
        {QStringLiteral("srcOutUs"), static_cast<double>(clip.srcOut)},
        {QStringLiteral("volume"), keyframesToJson(clip.volume)},
        {QStringLiteral("opacity"), keyframesToJson(clip.opacity)},
        {QStringLiteral("posX"), keyframesToJson(clip.posX)},
        {QStringLiteral("posY"), keyframesToJson(clip.posY)},
        {QStringLiteral("scale"), keyframesToJson(clip.scale)},
        {QStringLiteral("rotation"), keyframesToJson(clip.rotation)},
        {QStringLiteral("effects"), effectsToJson(clip.effects)},
    };
}

Clip clipFromJsonV2(const QJsonObject &object)
{
    Clip clip;
    clip.id = object.value(QStringLiteral("id")).toString(QUuid::createUuid().toString(QUuid::WithoutBraces));
    clip.assetId = object.value(QStringLiteral("assetId")).toString();
    clip.type = clipTypeFromString(object.value(QStringLiteral("type")).toString());
    clip.name = object.value(QStringLiteral("name")).toString();
    clip.textContent = object.value(QStringLiteral("textContent")).toString();
    clip.textStyle = textStyleFromJson(object.value(QStringLiteral("textStyle")).toObject());
    clip.path = object.value(QStringLiteral("path")).toString();
    clip.thumbnailPath = object.value(QStringLiteral("thumbnailPath")).toString();
    clip.filmstripPath = object.value(QStringLiteral("filmstripPath")).toString();
    clip.blendMode = blendModeFromString(object.value(QStringLiteral("blendMode")).toString());
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
    clip.posX = keyframesFromJson(object.value(QStringLiteral("posX")).toObject());
    clip.posY = keyframesFromJson(object.value(QStringLiteral("posY")).toObject());
    clip.scale = keyframesFromJson(object.value(QStringLiteral("scale")).toObject());
    clip.rotation = keyframesFromJson(object.value(QStringLiteral("rotation")).toObject());
    clip.effects = effectsFromJson(object.value(QStringLiteral("effects")).toArray());
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
        {.type = TrackType::Text},
        {.type = TrackType::Audio},
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

    const QJsonArray assetsArray = object.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &value : assetsArray) {
        const QJsonObject assetObject = value.toObject();
        if (version >= 2)
            project.addAsset(assetFromJsonV2(assetObject));
        else
            project.addAsset(assetFromJsonV1(assetObject));
    }

    project.resetToDefaultTimeline();
    const QJsonArray tracksArray = object.value(QStringLiteral("tracks")).toArray();
    for (int i = 0; i < tracksArray.size() && i < project.m_tracks.size(); ++i) {
        const QJsonObject trackObject = tracksArray.at(i).toObject();
        Track &track = project.m_tracks[i];
        track.type = trackTypeFromString(trackObject.value(QStringLiteral("type")).toString(trackTypeToString(track.type)));
        track.muted = trackObject.value(QStringLiteral("muted")).toBool(false);
        track.hidden = trackObject.value(QStringLiteral("hidden")).toBool(false);
        track.locked = trackObject.value(QStringLiteral("locked")).toBool(false);

        const QJsonArray clipsArray = trackObject.value(QStringLiteral("clips")).toArray();
        for (const QJsonValue &clipValue : clipsArray) {
            const QJsonObject clipObject = clipValue.toObject();
            if (version >= 2)
                track.clips.append(clipFromJsonV2(clipObject));
            else
                track.clips.append(clipFromJsonV1(clipObject, project.m_assetOrder));
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

        tracksArray.append(QJsonObject{
            {QStringLiteral("type"), trackTypeToString(track.type)},
            {QStringLiteral("muted"), track.muted},
            {QStringLiteral("hidden"), track.hidden},
            {QStringLiteral("locked"), track.locked},
            {QStringLiteral("clips"), clipsArray},
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
    };
}

} // namespace drift
