#include "AssetLibrary.h"

#include "core/Project.h"

#include "engine/MediaProbe.h"
#include "engine/MediaThumbnail.h"

#include <QFileInfo>
#include <QImageReader>
#include <QJsonObject>
#include <QUrl>
#include <QUuid>
#include <algorithm>

namespace {

bool isImagePath(const QString &path)
{
    static const QStringList extensions = {
        QStringLiteral("png"),  QStringLiteral("jpg"),  QStringLiteral("jpeg"),
        QStringLiteral("gif"),  QStringLiteral("webp"), QStringLiteral("bmp"),
        QStringLiteral("tiff"), QStringLiteral("tif"),  QStringLiteral("svg"),
    };
    return extensions.contains(QFileInfo(path).suffix().toLower());
}

drift::MediaKind kindFrom(const MediaInfo &info, const QString &path)
{
    if (isImagePath(path))
        return drift::MediaKind::Image;

    for (const StreamInfo &stream : info.streams) {
        if (stream.type == StreamInfo::Type::Video && !stream.attachedPicture)
            return drift::MediaKind::Video;
    }
    for (const StreamInfo &stream : info.streams) {
        if (stream.type == StreamInfo::Type::Audio)
            return drift::MediaKind::Audio;
    }
    return drift::MediaKind::Other;
}

QString formatDuration(drift::TimeUs durationUs)
{
    if (durationUs <= 0)
        return {};

    const int totalSeconds = static_cast<int>(durationUs / drift::kUsPerSecond);
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

} // namespace

AssetLibrary::AssetLibrary(QObject *parent)
    : QAbstractListModel(parent)
{
}

void AssetLibrary::setProject(drift::Project *project)
{
    beginResetModel();
    m_project = project;
    endResetModel();
}

int AssetLibrary::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_project)
        return 0;
    return m_project->assetOrder().size();
}

const drift::MediaAsset *AssetLibrary::assetAtIndex(int index) const
{
    if (!m_project || index < 0 || index >= m_project->assetOrder().size())
        return nullptr;
    return m_project->asset(m_project->assetIdAt(index));
}

drift::MediaAsset *AssetLibrary::assetAtIndex(int index)
{
    if (!m_project || index < 0 || index >= m_project->assetOrder().size())
        return nullptr;
    return m_project->asset(m_project->assetIdAt(index));
}

QVariant AssetLibrary::data(const QModelIndex &index, int role) const
{
    const drift::MediaAsset *asset = assetAtIndex(index.row());
    if (!index.isValid() || !asset)
        return {};

    switch (role) {
    case IdRole:
        return asset->id;
    case NameRole:
        return asset->name;
    case KindRole:
        return drift::mediaKindToString(asset->kind);
    case DurationRole:
        return asset->durationLabel;
    case DurationSecondsRole:
        return drift::usToSeconds(asset->durationUs);
    case PathRole:
        return asset->path;
    case ThumbnailPathRole:
        return asset->thumbnailPath;
    case FilmstripPathRole:
        return asset->filmstripPath;
    default:
        return {};
    }
}

QHash<int, QByteArray> AssetLibrary::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {KindRole, "kind"},
        {DurationRole, "duration"},
        {DurationSecondsRole, "durationSeconds"},
        {PathRole, "path"},
        {ThumbnailPathRole, "thumbnailPath"},
        {FilmstripPathRole, "filmstripPath"},
    };
}

bool AssetLibrary::containsPath(const QString &path) const
{
    return indexOfPath(path) >= 0;
}

int AssetLibrary::indexOfPath(const QString &path) const
{
    if (!m_project)
        return -1;

    const QString normalized = QFileInfo(path).absoluteFilePath();
    for (int i = 0; i < m_project->assetOrder().size(); ++i) {
        const drift::MediaAsset *asset = assetAtIndex(i);
        if (asset && asset->path == normalized)
            return i;
    }
    return -1;
}

int AssetLibrary::indexOfId(const QString &id) const
{
    if (!m_project)
        return -1;
    return m_project->assetIndex(id);
}

QString AssetLibrary::assetIdAt(int index) const
{
    if (!m_project)
        return {};
    return m_project->assetIdAt(index);
}

void AssetLibrary::refreshMediaAt(int index)
{
    drift::MediaAsset *asset = assetAtIndex(index);
    if (!asset)
        return;

    bool changed = false;

    if (asset->thumbnailPath.isEmpty() || !QFileInfo::exists(asset->thumbnailPath)) {
        const QString thumb = MediaThumbnail::generate(asset->path, drift::mediaKindToString(asset->kind));
        if (!thumb.isEmpty()) {
            asset->thumbnailPath = thumb;
            changed = true;
        }
    }

    if (asset->kind == drift::MediaKind::Video) {
        if (asset->filmstripPath.isEmpty() || !QFileInfo::exists(asset->filmstripPath)) {
            const QString strip = MediaThumbnail::generateFilmstrip(asset->path, drift::mediaKindToString(asset->kind));
            if (!strip.isEmpty()) {
                asset->filmstripPath = strip;
                changed = true;
            }
        }
    } else if (!asset->thumbnailPath.isEmpty() && asset->filmstripPath != asset->thumbnailPath) {
        asset->filmstripPath = asset->thumbnailPath;
        changed = true;
    }

    if (!changed)
        return;

    const QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {ThumbnailPathRole, FilmstripPathRole});
}

QVariantMap AssetLibrary::assetAt(int index) const
{
    const drift::MediaAsset *asset = assetAtIndex(index);
    if (!asset)
        return {};

    return {
        {QStringLiteral("id"), asset->id},
        {QStringLiteral("name"), asset->name},
        {QStringLiteral("kind"), drift::mediaKindToString(asset->kind)},
        {QStringLiteral("duration"), asset->durationLabel},
        {QStringLiteral("durationSeconds"), drift::usToSeconds(asset->durationUs)},
        {QStringLiteral("path"), asset->path},
        {QStringLiteral("width"), asset->width},
        {QStringLiteral("height"), asset->height},
        {QStringLiteral("fps"), asset->fps},
        {QStringLiteral("rotationDegrees"), asset->rotationDegrees},
        {QStringLiteral("thumbnailPath"), asset->thumbnailPath},
        {QStringLiteral("filmstripPath"), asset->filmstripPath},
        {QStringLiteral("assetIndex"), index},
    };
}

QString AssetLibrary::thumbnailAt(int index) const
{
    const drift::MediaAsset *asset = assetAtIndex(index);
    return asset ? asset->thumbnailPath : QString{};
}

QString AssetLibrary::filmstripAt(int index) const
{
    const drift::MediaAsset *asset = assetAtIndex(index);
    return asset ? asset->filmstripPath : QString{};
}

void AssetLibrary::ensureMedia(int index)
{
    refreshMediaAt(index);
}

void AssetLibrary::ensureAllMedia()
{
    if (!m_project)
        return;
    for (int i = 0; i < m_project->assetOrder().size(); ++i)
        refreshMediaAt(i);
}

void AssetLibrary::sortByName()
{
    if (!m_project || m_project->assetOrder().size() < 2)
        return;

    beginResetModel();
    QList<QString> order = m_project->assetOrder();
    std::sort(order.begin(), order.end(), [this](const QString &a, const QString &b) {
        const drift::MediaAsset *assetA = m_project->asset(a);
        const drift::MediaAsset *assetB = m_project->asset(b);
        if (!assetA || !assetB)
            return a < b;
        return assetA->name.compare(assetB->name, Qt::CaseInsensitive) < 0;
    });
    m_project->assetOrder() = order;
    endResetModel();
}

void AssetLibrary::sortByKind()
{
    if (!m_project || m_project->assetOrder().size() < 2)
        return;

    beginResetModel();
    QList<QString> order = m_project->assetOrder();
    std::sort(order.begin(), order.end(), [this](const QString &a, const QString &b) {
        const drift::MediaAsset *assetA = m_project->asset(a);
        const drift::MediaAsset *assetB = m_project->asset(b);
        if (!assetA || !assetB)
            return a < b;
        const int cmp = drift::mediaKindToString(assetA->kind)
                            .compare(drift::mediaKindToString(assetB->kind), Qt::CaseInsensitive);
        return cmp != 0 ? cmp < 0 : assetA->name.compare(assetB->name, Qt::CaseInsensitive) < 0;
    });
    m_project->assetOrder() = order;
    endResetModel();
}

void AssetLibrary::clear()
{
    if (!m_project || m_project->assetOrder().isEmpty())
        return;

    beginResetModel();
    m_project->assets().clear();
    m_project->assetOrder().clear();
    endResetModel();
}

QJsonArray AssetLibrary::toJsonArray() const
{
    if (!m_project)
        return {};

    QJsonArray assets;
    for (const QString &id : m_project->assetOrder()) {
        const drift::MediaAsset *asset = m_project->asset(id);
        if (!asset)
            continue;
        assets.append(QJsonObject{
            {QStringLiteral("id"), asset->id},
            {QStringLiteral("name"), asset->name},
            {QStringLiteral("kind"), drift::mediaKindToString(asset->kind)},
            {QStringLiteral("durationUs"), static_cast<double>(asset->durationUs)},
            {QStringLiteral("duration"), asset->durationLabel},
            {QStringLiteral("path"), asset->path},
            {QStringLiteral("width"), asset->width},
            {QStringLiteral("height"), asset->height},
            {QStringLiteral("fps"), asset->fps},
            {QStringLiteral("rotationDegrees"), asset->rotationDegrees},
            {QStringLiteral("sampleRate"), asset->sampleRate},
            {QStringLiteral("channels"), asset->channels},
            {QStringLiteral("codecName"), asset->codecName},
            {QStringLiteral("thumbnailPath"), asset->thumbnailPath},
            {QStringLiteral("filmstripPath"), asset->filmstripPath},
        });
    }
    return assets;
}

void AssetLibrary::loadFromJsonArray(const QJsonArray &assets)
{
    if (!m_project)
        return;

    beginResetModel();
    m_project->assets().clear();
    m_project->assetOrder().clear();

    for (const QJsonValue &value : assets) {
        const QJsonObject object = value.toObject();
        drift::MediaAsset asset;
        asset.id = object.value(QStringLiteral("id")).toString(QUuid::createUuid().toString(QUuid::WithoutBraces));
        asset.name = object.value(QStringLiteral("name")).toString();
        asset.kind = drift::mediaKindFromString(object.value(QStringLiteral("kind")).toString());
        asset.durationLabel = object.value(QStringLiteral("duration")).toString();
        if (object.contains(QStringLiteral("durationUs"))) {
            asset.durationUs = static_cast<drift::TimeUs>(object.value(QStringLiteral("durationUs")).toDouble());
        } else {
            asset.durationUs = drift::secondsToUs(object.value(QStringLiteral("durationSeconds")).toDouble());
        }
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
        m_project->addAsset(asset);
    }

    endResetModel();

    for (int i = 0; i < m_project->assetOrder().size(); ++i)
        refreshMediaAt(i);
}

void AssetLibrary::importUrls(const QList<QUrl> &urls)
{
    QStringList paths;
    paths.reserve(urls.size());
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        } else if (!url.isEmpty()) {
            const QString asString = url.toString();
            if (asString.startsWith(QLatin1String("file://"), Qt::CaseInsensitive))
                paths.append(QUrl(asString).toLocalFile());
            else
                paths.append(asString);
        }
    }
    importFiles(paths);
}

void AssetLibrary::importFiles(const QStringList &paths)
{
    if (!m_project)
        return;

    for (const QString &path : paths) {
        const QFileInfo fileInfo(path);
        const QString absolutePath = fileInfo.absoluteFilePath();
        if (!fileInfo.isFile())
            continue;

        const int existingIndex = indexOfPath(absolutePath);
        if (existingIndex >= 0) {
            refreshMediaAt(existingIndex);
            continue;
        }

        if (isImagePath(path)) {
            const QString kind = drift::mediaKindToString(drift::MediaKind::Image);
            const QString thumb = MediaThumbnail::generate(absolutePath, kind);
            QImageReader reader(absolutePath);
            reader.setAutoTransform(true);
            QSize size = reader.size();
            if (reader.transformation() & QImageIOHandler::TransformationRotate90)
                size.transpose();
            drift::MediaAsset asset;
            asset.name = fileInfo.fileName();
            asset.kind = drift::MediaKind::Image;
            asset.path = absolutePath;
            asset.width = size.width();
            asset.height = size.height();
            asset.thumbnailPath = thumb;
            asset.filmstripPath = thumb;
            const int row = m_project->assetOrder().size();
            beginInsertRows({}, row, row);
            m_project->addAsset(asset);
            endInsertRows();
            continue;
        }

        const MediaInfo info = MediaProbe::probe(absolutePath);
        if (!info.ok)
            continue;

        const drift::MediaKind kind = kindFrom(info, path);
        const QString kindString = drift::mediaKindToString(kind);
        const QString thumb = MediaThumbnail::generate(absolutePath, kindString);
        const QString strip = kind == drift::MediaKind::Video
                                  ? MediaThumbnail::generateFilmstrip(absolutePath, kindString)
                                  : thumb;

        drift::MediaAsset asset;
        asset.name = fileInfo.fileName();
        asset.kind = kind;
        asset.durationUs = info.durationUs;
        asset.durationLabel = kind == drift::MediaKind::Image ? QString() : formatDuration(info.durationUs);
        asset.path = absolutePath;
        asset.thumbnailPath = thumb;
        asset.filmstripPath = strip;

        for (const StreamInfo &stream : info.streams) {
            if (stream.type == StreamInfo::Type::Video && !stream.attachedPicture) {
                asset.width = stream.width;
                asset.height = stream.height;
                asset.fps = stream.fps;
                asset.rotationDegrees = stream.rotationDegrees;
                asset.codecName = stream.codecName;
            } else if (stream.type == StreamInfo::Type::Audio) {
                asset.sampleRate = stream.sampleRate;
                asset.channels = stream.channels;
                if (asset.codecName.isEmpty())
                    asset.codecName = stream.codecName;
            }
        }

        const int row = m_project->assetOrder().size();
        beginInsertRows({}, row, row);
        m_project->addAsset(asset);
        endInsertRows();
    }
}
