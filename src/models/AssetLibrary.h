#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QStringList>
#include <QUrl>

class AssetLibrary : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        KindRole,
        DurationRole,
        DurationSecondsRole,
        PathRole,
        ThumbnailPathRole,
    };
    Q_ENUM(Role)

    explicit AssetLibrary(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void importUrls(const QList<QUrl> &urls);
    Q_INVOKABLE QVariantMap assetAt(int index) const;
    Q_INVOKABLE QString thumbnailAt(int index) const;

    QJsonArray toJsonArray() const;
    void loadFromJsonArray(const QJsonArray &assets);
    void clear();

private:
    struct Asset {
        QString name;
        QString kind;
        QString duration;
        double durationSeconds = 0.0;
        QString path;
        QString thumbnailPath;
    };

    void importFiles(const QStringList &paths);
    bool containsPath(const QString &path) const;
    void appendAsset(Asset asset);

    QList<Asset> m_assets;
};
