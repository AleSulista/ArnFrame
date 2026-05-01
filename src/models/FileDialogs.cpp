#include "FileDialogs.h"

#include <QFileDialog>

FileDialogs::FileDialogs(QObject *parent) : QObject(parent) {}

QUrl FileDialogs::openFile(const QString &title, const QStringList &nameFilters) const
{
    QFileDialog dialog;
    dialog.setWindowTitle(title);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (!nameFilters.isEmpty())
        dialog.setNameFilters(nameFilters);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    const QList<QUrl> urls = dialog.selectedUrls();
    return urls.isEmpty() ? QUrl() : urls.first();
}

QList<QUrl> FileDialogs::openFiles(const QString &title, const QStringList &nameFilters) const
{
    QFileDialog dialog;
    dialog.setWindowTitle(title);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    if (!nameFilters.isEmpty())
        dialog.setNameFilters(nameFilters);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return dialog.selectedUrls();
}

QUrl FileDialogs::saveFile(const QString &title, const QStringList &nameFilters,
                           const QString &defaultSuffix) const
{
    QFileDialog dialog;
    dialog.setWindowTitle(title);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    if (!nameFilters.isEmpty())
        dialog.setNameFilters(nameFilters);
    if (!defaultSuffix.isEmpty())
        dialog.setDefaultSuffix(defaultSuffix);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    const QList<QUrl> urls = dialog.selectedUrls();
    return urls.isEmpty() ? QUrl() : urls.first();
}
