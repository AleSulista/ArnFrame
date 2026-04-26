#include "AssetLibrary.h"

#include "engine/MediaProbe.h"
#include "engine/MediaThumbnail.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QUrl>

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

QString kindFrom(const MediaInfo &info, const QString &path)
{
    if (isImagePath(path))
        return QStringLiteral("image");

    for (const StreamInfo &stream : info.streams) {
        if (stream.type == StreamInfo::Type::Video)
            return QStringLiteral("video");
    }
    for (const StreamInfo &stream : info.streams) {
        if (stream.type == StreamInfo::Type::Audio)
            return QStringLiteral("audio");
    }
    return QStringLiteral("other");
}

QString formatDuration(int64_t durationUs)
{
    if (durationUs <= 0)
        return {};

    const int totalSeconds = static_cast<int>(durationUs / 1'000'000);
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

int AssetLibrary::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_assets.size();
}

QVariant AssetLibrary::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_assets.size())
        return {};

    const Asset &asset = m_assets.at(index.row());
    switch (role) {
    case NameRole:
        return asset.name;
    case KindRole:
        return asset.kind;
    case DurationRole:
        return asset.duration;
    case DurationSecondsRole:
        return asset.durationSeconds;
    case PathRole:
        return asset.path;
    case ThumbnailPathRole:
        return asset.thumbnailPath;
    case FilmstripPathRole:
        return asset.filmstripPath;
    default:
        return {};
    }
}

QHash<int, QByteArray> AssetLibrary::roleNames() const
{
    return {
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
    const QString normalized = QFileInfo(path).absoluteFilePath();
    for (int i = 0; i < m_assets.size(); ++i) {
        if (m_assets.at(i).path == normalized)
            return i;
    }
    return -1;
}

void AssetLibrary::refreshMediaAt(int index)
{
    if (index < 0 || index >= m_assets.size())
        return;

    Asset &asset = m_assets[index];
    bool changed = false;

    if (asset.thumbnailPath.isEmpty() || !QFileInfo::exists(asset.thumbnailPath)) {
        const QString thumb = MediaThumbnail::generate(asset.path, asset.kind);
        if (!thumb.isEmpty()) {
            asset.thumbnailPath = thumb;
            changed = true;
        }
    }

    if (asset.kind == QStringLiteral("video")) {
        if (asset.filmstripPath.isEmpty() || !QFileInfo::exists(asset.filmstripPath)) {
            const QString strip = MediaThumbnail::generateFilmstrip(asset.path, asset.kind);
            if (!strip.isEmpty()) {
                asset.filmstripPath = strip;
                changed = true;
            }
        }
    } else if (!asset.thumbnailPath.isEmpty() && asset.filmstripPath != asset.thumbnailPath) {
        asset.filmstripPath = asset.thumbnailPath;
        changed = true;
    }

    if (!changed)
        return;

    const QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {ThumbnailPathRole, FilmstripPathRole});
}

void AssetLibrary::appendAsset(Asset asset)
{
    const int row = m_assets.size();
    beginInsertRows({}, row, row);
    m_assets.append(std::move(asset));
    endInsertRows();
}

QVariantMap AssetLibrary::assetAt(int index) const
{
    if (index < 0 || index >= m_assets.size())
        return {};

    const Asset &asset = m_assets.at(index);
    return {
        {QStringLiteral("name"), asset.name},
        {QStringLiteral("kind"), asset.kind},
        {QStringLiteral("duration"), asset.duration},
        {QStringLiteral("durationSeconds"), asset.durationSeconds},
        {QStringLiteral("path"), asset.path},
        {QStringLiteral("thumbnailPath"), asset.thumbnailPath},
        {QStringLiteral("filmstripPath"), asset.filmstripPath},
    };
}

QString AssetLibrary::thumbnailAt(int index) const
{
    if (index < 0 || index >= m_assets.size())
        return {};
    return m_assets.at(index).thumbnailPath;
}

QString AssetLibrary::filmstripAt(int index) const
{
    if (index < 0 || index >= m_assets.size())
        return {};
    return m_assets.at(index).filmstripPath;
}

void AssetLibrary::ensureMedia(int index)
{
    refreshMediaAt(index);
}

void AssetLibrary::ensureAllMedia()
{
    for (int i = 0; i < m_assets.size(); ++i)
        refreshMediaAt(i);
}

void AssetLibrary::clear()
{
    if (m_assets.isEmpty())
        return;

    beginResetModel();
    m_assets.clear();
    endResetModel();
}

QJsonArray AssetLibrary::toJsonArray() const
{
    QJsonArray assets;
    for (const Asset &asset : m_assets) {
        assets.append(QJsonObject{
            {QStringLiteral("name"), asset.name},
            {QStringLiteral("kind"), asset.kind},
            {QStringLiteral("duration"), asset.duration},
            {QStringLiteral("durationSeconds"), asset.durationSeconds},
            {QStringLiteral("path"), asset.path},
            {QStringLiteral("thumbnailPath"), asset.thumbnailPath},
            {QStringLiteral("filmstripPath"), asset.filmstripPath},
        });
    }
    return assets;
}

void AssetLibrary::loadFromJsonArray(const QJsonArray &assets)
{
    beginResetModel();
    m_assets.clear();

    for (const QJsonValue &value : assets) {
        const QJsonObject object = value.toObject();
        m_assets.append({
            .name = object.value(QStringLiteral("name")).toString(),
            .kind = object.value(QStringLiteral("kind")).toString(),
            .duration = object.value(QStringLiteral("duration")).toString(),
            .durationSeconds = object.value(QStringLiteral("durationSeconds")).toDouble(),
            .path = object.value(QStringLiteral("path")).toString(),
            .thumbnailPath = object.value(QStringLiteral("thumbnailPath")).toString(),
            .filmstripPath = object.value(QStringLiteral("filmstripPath")).toString(),
        });
    }

    endResetModel();

    for (int i = 0; i < m_assets.size(); ++i)
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
            const QString kind = QStringLiteral("image");
            const QString thumb = MediaThumbnail::generate(absolutePath, kind);
            appendAsset({
                .name = fileInfo.fileName(),
                .kind = kind,
                .duration = {},
                .durationSeconds = 0.0,
                .path = absolutePath,
                .thumbnailPath = thumb,
                .filmstripPath = thumb,
            });
            continue;
        }

        const MediaInfo info = MediaProbe::probe(absolutePath);
        if (!info.ok)
            continue;

        const QString kind = kindFrom(info, path);
        const double durationSeconds = info.durationUs > 0 ? info.durationUs / 1'000'000.0 : 0.0;
        const QString thumb = MediaThumbnail::generate(absolutePath, kind);
        const QString strip = kind == QStringLiteral("video")
                                  ? MediaThumbnail::generateFilmstrip(absolutePath, kind)
                                  : thumb;
        appendAsset({
            .name = fileInfo.fileName(),
            .kind = kind,
            .duration = kind == QStringLiteral("image") ? QString() : formatDuration(info.durationUs),
            .durationSeconds = durationSeconds,
            .path = absolutePath,
            .thumbnailPath = thumb,
            .filmstripPath = strip,
        });
    }
}
