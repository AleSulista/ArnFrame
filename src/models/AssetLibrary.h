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
        FilmstripPathRole,
    };
    Q_ENUM(Role)

    explicit AssetLibrary(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void importUrls(const QList<QUrl> &urls);
    Q_INVOKABLE QVariantMap assetAt(int index) const;
    Q_INVOKABLE QString thumbnailAt(int index) const;
    Q_INVOKABLE QString filmstripAt(int index) const;
    Q_INVOKABLE void ensureMedia(int index);
    Q_INVOKABLE void ensureAllMedia();
    Q_INVOKABLE void sortByName();
    Q_INVOKABLE void sortByKind();

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
        QString filmstripPath;
    };

    void importFiles(const QStringList &paths);
    bool containsPath(const QString &path) const;
    void appendAsset(Asset asset);
    int indexOfPath(const QString &path) const;
    void refreshMediaAt(int index);

    QList<Asset> m_assets;
};
