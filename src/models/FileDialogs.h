#pragma once

#include <QList>
#include <QObject>
#include <QStringList>
#include <QUrl>

// QML-facing wrapper around QFileDialog so file pickers use the native
// platform dialog (and xdg-desktop-portal under Flatpak) instead of the
// QtQuick.Dialogs QML fallback.
class FileDialogs : public QObject
{
    Q_OBJECT

public:
    explicit FileDialogs(QObject *parent = nullptr);

    Q_INVOKABLE QUrl openFile(const QString &title, const QStringList &nameFilters) const;
    Q_INVOKABLE QList<QUrl> openFiles(const QString &title, const QStringList &nameFilters) const;
    Q_INVOKABLE QUrl saveFile(const QString &title, const QStringList &nameFilters,
                              const QString &defaultSuffix = QString()) const;
};
