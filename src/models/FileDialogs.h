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

    Q_INVOKABLE QUrl openFile(const QString &title, const QStringList &nameFilters,
                              const QStringList &mimeTypeFilters = QStringList()) const;
    Q_INVOKABLE QList<QUrl> openFiles(const QString &title, const QStringList &nameFilters) const;
    // `suffix` is appended to `suggestedName` for the picker's initial file name; the path the
    // dialog returns is used exactly as given. `initialDirectory` opens the picker in that folder
    // when it exists (e.g. the last export location). `mimeTypeFilters` are used when those types
    // are in the MIME database (so the portal can label a new .drift); otherwise `nameFilters`.
    Q_INVOKABLE QUrl saveFile(const QString &title, const QStringList &nameFilters,
                              const QString &suggestedName = QString(),
                              const QString &suffix = QString(),
                              const QString &initialDirectory = QString(),
                              const QStringList &mimeTypeFilters = QStringList()) const;
};
