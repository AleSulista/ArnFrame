#include "AssetLibrary.h"

#include "engine/MediaProbe.h"

#include <QFileInfo>

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
    case PathRole:
        return asset.path;
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
        {PathRole, "path"},
    };
}

bool AssetLibrary::containsPath(const QString &path) const
{
    for (const Asset &asset : m_assets) {
        if (asset.path == path)
            return true;
    }
    return false;
}

void AssetLibrary::appendAsset(Asset asset)
{
    const int row = m_assets.size();
    beginInsertRows({}, row, row);
    m_assets.append(std::move(asset));
    endInsertRows();
}

void AssetLibrary::importUrls(const QList<QUrl> &urls)
{
    QStringList paths;
    paths.reserve(urls.size());
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
        else if (!url.isEmpty())
            paths.append(url.toString(QUrl::PreferLocalFile));
    }
    importFiles(paths);
}

void AssetLibrary::importFiles(const QStringList &paths)
{
    for (const QString &path : paths) {
        const QFileInfo fileInfo(path);
        if (!fileInfo.isFile() || containsPath(path))
            continue;

        if (isImagePath(path)) {
            appendAsset({
                .name = fileInfo.fileName(),
                .kind = QStringLiteral("image"),
                .duration = {},
                .path = fileInfo.absoluteFilePath(),
            });
            continue;
        }

        const MediaInfo info = MediaProbe::probe(fileInfo.absoluteFilePath());
        if (!info.ok)
            continue;

        const QString kind = kindFrom(info, path);
        appendAsset({
            .name = fileInfo.fileName(),
            .kind = kind,
            .duration = kind == QStringLiteral("image") ? QString() : formatDuration(info.durationUs),
            .path = fileInfo.absoluteFilePath(),
        });
    }
}
