#include "FileDialogs.h"

#include <QDir>
#include <QFileDialog>
#include <QMimeDatabase>
#include <QMimeType>

namespace {

void applyFilters(QFileDialog &dialog, const QStringList &nameFilters,
                  const QStringList &mimeTypeFilters)
{
    if (!mimeTypeFilters.isEmpty()) {
        QMimeDatabase db;
        bool allKnown = true;
        for (const QString &mime : mimeTypeFilters) {
            if (db.mimeTypeForName(mime).isValid())
                continue;
            allKnown = false;
            break;
        }
        if (allKnown) {
            dialog.setMimeTypeFilters(mimeTypeFilters);
            return;
        }
    }
    if (!nameFilters.isEmpty())
        dialog.setNameFilters(nameFilters);
}

} // namespace

FileDialogs::FileDialogs(QObject *parent) : QObject(parent) {}

QUrl FileDialogs::openFile(const QString &title, const QStringList &nameFilters,
                           const QStringList &mimeTypeFilters) const
{
    QFileDialog dialog;
    dialog.setWindowTitle(title);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    applyFilters(dialog, nameFilters, mimeTypeFilters);
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
                           const QString &suggestedName, const QString &suffix,
                           const QString &initialDirectory, const QStringList &mimeTypeFilters) const
{
    QFileDialog dialog;
    dialog.setWindowTitle(title);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    applyFilters(dialog, nameFilters, mimeTypeFilters);

    if (!initialDirectory.isEmpty() && QDir(initialDirectory).exists())
        dialog.setDirectory(initialDirectory);

    // The extension is put in the suggested name instead of QFileDialog::setDefaultSuffix: a file
    // exported through the documents portal must not be renamed afterwards, and appending the
    // suffix to what the portal returned writes to a path the portal never registered — the data
    // lands next to the picked file as a hidden entry instead of at the chosen name.
    QString name = suggestedName.trimmed();
    name.replace(QLatin1Char('/'), QLatin1Char('_'));
    name.replace(QLatin1Char('\\'), QLatin1Char('_'));
    if (name.isEmpty())
        name = tr("Untitled");
    if (!suffix.isEmpty())
        name += QLatin1Char('.') + suffix;
    dialog.selectFile(name);

    if (dialog.exec() != QDialog::Accepted)
        return {};
    const QList<QUrl> urls = dialog.selectedUrls();
    return urls.isEmpty() ? QUrl() : urls.first();
}
