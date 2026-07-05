#include "AddonManager.h"

#include "AddonEndpoint.h"
#include "engine/AddonPackage.h"
#include "engine/AddonRegistry.h"
#include "engine/FontCatalog.h"
#include "engine/StickerCatalog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QtConcurrent>

#include <atomic>
#include <utility>

using namespace drift::addon;

namespace {

constexpr qint64 kIndexMaxAgeSeconds = 6 * 60 * 60;

QString cachedIndexPath()
{
    return QDir(addonsDir()).filePath(QStringLiteral("index.json"));
}

// Compare dotted numeric versions; trailing pre-release text is ignored, which is enough for the
// "is there something newer" question the manager actually asks.
int compareVersions(const QString &a, const QString &b)
{
    const QStringList left = a.split(QLatin1Char('.'));
    const QStringList right = b.split(QLatin1Char('.'));
    for (int i = 0; i < qMax(left.size(), right.size()); ++i) {
        const int l = left.value(i).section(QLatin1Char('-'), 0, 0).toInt();
        const int r = right.value(i).section(QLatin1Char('-'), 0, 0).toInt();
        if (l != r)
            return l < r ? -1 : 1;
    }
    return 0;
}

QStringList kindsOf(const QJsonObject &addon)
{
    QStringList kinds;
    for (const QJsonValue &value : addon.value(QStringLiteral("provides")).toArray()) {
        const QString kind = value.toObject().value(QStringLiteral("kind")).toString();
        if (!kind.isEmpty())
            kinds.append(kind);
    }
    if (kinds.isEmpty())
        kinds.append(addon.value(QStringLiteral("kind")).toString());
    return kinds;
}

} // namespace

// Everything in flight for one addon. Held by shared_ptr so the extraction lambda can read the
// cancel flag after the manager has dropped its own reference.
struct AddonManager::Transfer
{
    QString id;
    QString version;
    QString packagePath;
    QPointer<QNetworkReply> reply;
    QFile file;
    std::atomic_bool cancelled{false};
    bool extracting = false;
};

AddonManager::AddonManager(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this))
{
    QDir().mkpath(addonDownloadCacheDir());
    sweepDownloadCache();

    // Serve whatever we already know immediately, then go and check.
    QFile cached(cachedIndexPath());
    if (cached.open(QIODevice::ReadOnly))
        applyIndex(cached.readAll(), true);

    refresh();
}

AddonManager::~AddonManager() = default;

QString AddonManager::status() const
{
    return m_status;
}

bool AddonManager::refreshing() const
{
    return m_refreshing;
}

void AddonManager::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void AddonManager::setRefreshing(bool refreshing)
{
    if (m_refreshing == refreshing)
        return;
    m_refreshing = refreshing;
    emit refreshingChanged();
}

QVariantList AddonManager::catalog() const
{
    QVariantList rows;
    for (const QJsonObject &addon : m_remote) {
        const QString id = addon.value(QStringLiteral("id")).toString();
        const QString version = addon.value(QStringLiteral("version")).toString();
        const InstalledAddon *installed = installedAddon(id);

        QString state = QStringLiteral("available");
        if (const auto transfer = m_transfers.value(id))
            state = transfer->extracting ? QStringLiteral("installing") : QStringLiteral("downloading");
        else if (m_failures.contains(id))
            state = QStringLiteral("failed");
        else if (installed)
            state = compareVersions(installed->version, version) < 0 ? QStringLiteral("update-available")
                                                                     : QStringLiteral("installed");

        int items = 0;
        for (const QJsonValue &value : addon.value(QStringLiteral("provides")).toArray())
            items += value.toObject().value(QStringLiteral("items")).toInt();

        rows.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), addon.value(QStringLiteral("name")).toString()},
            {QStringLiteral("description"), addon.value(QStringLiteral("description")).toString()},
            {QStringLiteral("author"), addon.value(QStringLiteral("author")).toString()},
            {QStringLiteral("license"), addon.value(QStringLiteral("license")).toString()},
            {QStringLiteral("kind"), addon.value(QStringLiteral("kind")).toString()},
            {QStringLiteral("version"), version},
            {QStringLiteral("installedVersion"), installed ? installed->version : QString()},
            {QStringLiteral("downloadSize"), addon.value(QStringLiteral("downloadSize")).toDouble()},
            {QStringLiteral("installedSize"), addon.value(QStringLiteral("installedSize")).toDouble()},
            {QStringLiteral("items"), items},
            {QStringLiteral("state"), state},
            {QStringLiteral("error"), m_failures.value(id)},
        });
    }
    return rows;
}

bool AddonManager::hasKind(const QString &kind) const
{
    return !addonRootsForKind(kind).isEmpty();
}

QString AddonManager::firstAddonForKind(const QString &kind) const
{
    for (const QJsonObject &addon : m_remote) {
        if (kindsOf(addon).contains(kind))
            return addon.value(QStringLiteral("id")).toString();
    }
    return {};
}

void AddonManager::refresh(bool force)
{
    if (m_refreshing)
        return;

    if (!addonServiceConfigured()) {
        setStatus(QStringLiteral("This build has no addon service configured."));
        return;
    }

    if (!force) {
        const QFileInfo cached(cachedIndexPath());
        if (cached.exists() && cached.lastModified().secsTo(QDateTime::currentDateTime()) < kIndexMaxAgeSeconds)
            return;
    }

    setRefreshing(true);
    QNetworkRequest request{QUrl(kIndexUrl)};
    request.setRawHeader("X-Drift-Client", kClientToken.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        setRefreshing(false);

        if (reply->error() != QNetworkReply::NoError) {
            // Non-fatal: whatever is installed keeps working, and the cached index still lists it.
            setStatus(QStringLiteral("Could not reach the addon service: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray body = reply->readAll();
        QSaveFile cache(cachedIndexPath());
        if (cache.open(QIODevice::WriteOnly)) {
            cache.write(body);
            cache.commit();
        }
        applyIndex(body, false);
        setStatus(QString());

        const QStringList waiting = std::exchange(m_awaitingFreshIndex, {});
        for (const QString &id : waiting)
            startDownload(id);
    });
}

void AddonManager::applyIndex(const QByteArray &json, bool fromCache)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (root.value(QStringLiteral("schema")).toInt() != 1) {
        if (!fromCache)
            setStatus(QStringLiteral("The addon service returned something this build cannot read."));
        return;
    }

    m_remote.clear();
    for (const QJsonValue &value : root.value(QStringLiteral("addons")).toArray())
        m_remote.append(value.toObject());

    emit catalogChanged();
}

void AddonManager::install(const QString &id)
{
    if (m_transfers.contains(id) || !addonServiceConfigured())
        return;

    m_failures.remove(id);
    startDownload(id);
}

void AddonManager::startDownload(const QString &id)
{
    QJsonObject addon;
    for (const QJsonObject &candidate : m_remote) {
        if (candidate.value(QStringLiteral("id")).toString() == id) {
            addon = candidate;
            break;
        }
    }

    const QString url = addon.value(QStringLiteral("url")).toString();
    if (url.isEmpty()) {
        if (!m_awaitingFreshIndex.contains(id))
            m_awaitingFreshIndex.append(id);
        refresh(true);
        return;
    }

    auto transfer = std::make_shared<Transfer>();
    transfer->id = id;
    transfer->version = addon.value(QStringLiteral("version")).toString();
    transfer->packagePath = QDir(addonDownloadCacheDir())
                                .filePath(QStringLiteral("%1-%2.driftpkg").arg(id, transfer->version));

    // Resume where a previous attempt stopped rather than re-fetching hundreds of megabytes.
    const QString partial = transfer->packagePath + QStringLiteral(".part");
    const qint64 have = QFileInfo(partial).size();
    transfer->file.setFileName(partial);
    if (!transfer->file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_failures.insert(id, QStringLiteral("Cannot write to the download cache"));
        emit catalogChanged();
        return;
    }

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    if (have > 0)
        request.setRawHeader("Range", QByteArrayLiteral("bytes=") + QByteArray::number(have) + "-");

    transfer->reply = m_network->get(request);
    m_transfers.insert(id, transfer);
    emit catalogChanged();

    QNetworkReply *reply = transfer->reply;
    const double total = addon.value(QStringLiteral("downloadSize")).toDouble();

    connect(reply, &QNetworkReply::readyRead, this, [this, transfer] {
        if (transfer->reply)
            transfer->file.write(transfer->reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, id, have, total](qint64 received, qint64) {
                const double done = double(have + received);
                emit progressChanged(id, total > 0 ? qMin(1.0, done / total) : 0.0,
                                     QStringLiteral("Downloading"));
            });
    connect(reply, &QNetworkReply::finished, this, [this, id] { finishDownload(id); });
}

void AddonManager::finishDownload(const QString &id)
{
    const auto transfer = m_transfers.value(id);
    if (!transfer)
        return;

    QNetworkReply *reply = transfer->reply;
    if (!reply)
        return;
    reply->deleteLater();

    transfer->file.write(reply->readAll());
    transfer->file.close();

    if (transfer->cancelled) {
        m_transfers.remove(id);
        emit catalogChanged();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_transfers.remove(id);

        // A rejected ticket means the index we started from has gone stale, not that anything is
        // wrong with the addon — fetch a fresh one and pick the download back up where it stopped.
        if (status == 403 && !m_retried.contains(id)) {
            m_retried.insert(id);
            m_awaitingFreshIndex.append(id);
            emit catalogChanged();
            refresh(true);
            return;
        }

        m_transfers.insert(id, transfer);
        // The .part file is deliberately left in place so the next attempt resumes.
        failTransfer(id, reply->errorString());
        return;
    }
    m_retried.remove(id);

    const QString partial = transfer->packagePath + QStringLiteral(".part");
    QFile::remove(transfer->packagePath);
    if (!QFile::rename(partial, transfer->packagePath)) {
        failTransfer(id, QStringLiteral("Could not finalise the download"));
        return;
    }

    beginExtract(id, transfer->packagePath);
}

void AddonManager::beginExtract(const QString &id, const QString &packagePath)
{
    const auto transfer = m_transfers.value(id);
    if (!transfer)
        return;

    transfer->extracting = true;
    emit catalogChanged();
    emit progressChanged(id, 0.0, QStringLiteral("Installing"));

    const QString destination = addonInstallDir(id);

    // Verification and decompression are seconds of work on a large model, so they never run on
    // the GUI thread. The lambda owns a copy of the shared_ptr, so the cancel flag stays alive.
    auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this,
            [this, id, packagePath, watcher] {
                watcher->deleteLater();
                const auto [ok, message] = watcher->result();
                const auto finished = m_transfers.value(id);
                m_transfers.remove(id);
                QFile::remove(packagePath);

                if (!ok) {
                    if (finished && finished->cancelled) {
                        emit catalogChanged();
                        return;
                    }
                    m_failures.insert(id, message);
                    setStatus(message);
                    emit catalogChanged();
                    return;
                }

                reloadAddonRegistry();
                const InstalledAddon *installed = installedAddon(id);
                QStringList kinds;
                if (installed) {
                    for (const InstalledProvide &provide : installed->provides)
                        kinds.append(provide.kind);
                }
                reloadForKinds(kinds);
                emit catalogChanged();
            });

    watcher->setFuture(QtConcurrent::run([transfer, packagePath, destination]() -> QPair<bool, QString> {
        PackageInfo info;
        QString error;
        const bool ok = drift::addon::install(
            packagePath, destination,
            [transfer](qint64, qint64) { return !transfer->cancelled; }, &info, &error);
        if (!ok)
            return {false, error};
        if (!recordInstalledAddon(info, &error))
            return {false, error};
        return {true, QString()};
    }));
}

void AddonManager::failTransfer(const QString &id, const QString &message)
{
    m_transfers.remove(id);
    m_failures.insert(id, message);
    setStatus(message);
    emit catalogChanged();
}

void AddonManager::cancel(const QString &id)
{
    const auto transfer = m_transfers.value(id);
    if (!transfer)
        return;

    transfer->cancelled = true;
    if (transfer->reply)
        transfer->reply->abort(); // the finished handler tidies up
}

void AddonManager::uninstall(const QString &id)
{
    const InstalledAddon *installed = installedAddon(id);
    if (!installed)
        return;

    QStringList kinds;
    for (const InstalledProvide &provide : installed->provides)
        kinds.append(provide.kind);

    QString error;
    QDir(addonInstallDir(id)).removeRecursively();
    forgetInstalledAddon(id, &error);
    reloadAddonRegistry();
    reloadForKinds(kinds);
    m_failures.remove(id);
    emit catalogChanged();
}

void AddonManager::reloadForKinds(const QStringList &kinds)
{
    for (const QString &kind : kinds) {
        // reloadFontCatalog touches QFontDatabase, so it has to be here on the GUI thread rather
        // than on the extraction worker — same constraint as the call in main().
        if (kind == QLatin1String("fonts"))
            reloadFontCatalog();
        else if (kind == QLatin1String("stickers"))
            reloadStickerCatalog();
        // whisper-model and sam2-model need nothing: sessions are created lazily on next use.
        emit kindChanged(kind);
    }
}

void AddonManager::sweepDownloadCache()
{
    // Completed .driftpkg files are removed after a successful install, so anything left here is
    // from a crash. Half-finished .part files are kept — the next install resumes them.
    QDir cache(addonDownloadCacheDir());
    const QStringList stale = cache.entryList({QStringLiteral("*.driftpkg")}, QDir::Files);
    for (const QString &name : stale)
        QFile::remove(cache.filePath(name));
}
