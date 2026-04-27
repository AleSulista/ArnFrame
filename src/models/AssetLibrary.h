#pragma once

#include "core/MediaAsset.h"

#include <QAbstractListModel>
#include <QJsonArray>
#include <QStringList>
#include <QUrl>

namespace drift {
class Project;
}

// Media bin model backed by the project's asset table.
class AssetLibrary : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        KindRole,
        DurationRole,
        DurationSecondsRole,
        PathRole,
        ThumbnailPathRole,
        FilmstripPathRole,
    };
    Q_ENUM(Role)

    explicit AssetLibrary(QObject *parent = nullptr);

    void setProject(drift::Project *project);
    drift::Project *project() const { return m_project; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void importUrls(const QList<QUrl> &urls);
    Q_INVOKABLE QVariantMap assetAt(int index) const;
    Q_INVOKABLE QString assetIdAt(int index) const;
    Q_INVOKABLE int indexOfId(const QString &id) const;
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
    void importFiles(const QStringList &paths);
    bool containsPath(const QString &path) const;
    int indexOfPath(const QString &path) const;
    void refreshMediaAt(int index);
    const drift::MediaAsset *assetAtIndex(int index) const;
    drift::MediaAsset *assetAtIndex(int index);

    drift::Project *m_project = nullptr;
};
