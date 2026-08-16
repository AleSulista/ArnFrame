#include "mcp/McpDispatcher.h"
#include "mcp/McpCatalog.h"
#include "mcp/McpJson.h"

#include "models/AppController.h"
#include "models/AssetLibrary.h"

#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

namespace drift::mcp {
namespace {

QSet<QString> clipIdSet(AppController *c)
{
    QSet<QString> ids;
    const auto tracks = c->tracks();
    for (int t = 0; t < tracks.size(); ++t) {
        const auto clips = tracks.at(t).toMap().value(QStringLiteral("clips")).toList();
        for (const QVariant &clip : clips)
            ids.insert(clip.toMap().value(QStringLiteral("id")).toString());
    }
    return ids;
}

int jsonInt(const QJsonValue &v, int fallback = -1)
{
    if (v.isDouble())
        return v.toInt(fallback);
    if (v.isString()) {
        bool ok = false;
        const int n = v.toString().toInt(&ok);
        return ok ? n : fallback;
    }
    return fallback;
}

double jsonNumber(const QJsonValue &v, double fallback)
{
    if (v.isDouble())
        return v.toDouble(fallback);
    if (v.isString()) {
        bool ok = false;
        const double n = v.toString().toDouble(&ok);
        return ok ? n : fallback;
    }
    return fallback;
}

bool jsonBool(const QJsonValue &v, bool fallback = false)
{
    if (v.isBool())
        return v.toBool();
    if (v.isDouble())
        return v.toInt() != 0;
    if (v.isString()) {
        const QString s = v.toString().toLower();
        if (s == QLatin1String("true") || s == QLatin1String("1"))
            return true;
        if (s == QLatin1String("false") || s == QLatin1String("0"))
            return false;
    }
    return fallback;
}

QStringList jsonStringList(const QJsonValue &v)
{
    QStringList out;
    if (v.isString()) {
        out.append(v.toString());
        return out;
    }
    if (v.isArray()) {
        for (const QJsonValue &item : v.toArray()) {
            if (item.isString())
                out.append(item.toString());
        }
    }
    return out;
}

QJsonObject compactCatalogItem(const QVariantMap &item)
{
    QJsonObject o;
    const QString id = item.value(QStringLiteral("id")).toString();
    if (!id.isEmpty())
        o.insert(QStringLiteral("id"), id);
    const QString label = item.value(QStringLiteral("label")).toString();
    if (!label.isEmpty())
        o.insert(QStringLiteral("label"), label);
    else if (item.contains(QStringLiteral("displayName")))
        o.insert(QStringLiteral("label"), item.value(QStringLiteral("displayName")).toString());
    const QString category = item.value(QStringLiteral("category")).toString();
    if (!category.isEmpty())
        o.insert(QStringLiteral("cat"), category);
    return o;
}

} // namespace

McpDispatcher::McpDispatcher(AppController *controller)
    : m_controller(controller)
{
}

McpDispatcher::ClipRef McpDispatcher::resolveClip(const QJsonObject &args) const
{
    ClipRef ref;
    const QString id = args.value(QStringLiteral("clip")).toString().trimmed();
    if (!id.isEmpty()) {
        const QPair<int, int> loc = m_controller->mcpLocateClip(id);
        ref.track = loc.first;
        ref.clip = loc.second;
        ref.id = id;
        return ref;
    }
    ref.track = jsonInt(args.value(QStringLiteral("track")));
    ref.clip = jsonInt(args.value(QStringLiteral("index")));
    if (ref.valid()) {
        const QVariantMap clip = m_controller->mcpCompactClip(ref.track, ref.clip);
        ref.id = clip.value(QStringLiteral("id")).toString();
        if (ref.id.isEmpty())
            return {};
    }
    return ref;
}

int McpDispatcher::resolveAsset(const QJsonValue &value) const
{
    AssetLibrary *lib = m_controller->assetLibrary();
    if (!lib)
        return -1;
    if (value.isDouble()) {
        const int index = value.toInt(-1);
        return lib->assetAt(index).isEmpty() ? -1 : index;
    }
    const QString key = value.toString().trimmed();
    if (key.isEmpty())
        return -1;
    bool asInt = false;
    const int numeric = key.toInt(&asInt);
    if (asInt && !key.contains(QLatin1Char('-'))) {
        if (!lib->assetAt(numeric).isEmpty())
            return numeric;
    }
    const int byId = lib->indexOfId(key);
    if (byId >= 0)
        return byId;
    for (int i = 0; i < lib->count(); ++i) {
        if (lib->assetAt(i).value(QStringLiteral("name")).toString().compare(key, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

QJsonObject McpDispatcher::clipFeedback(const ClipRef &ref, const QJsonObject &extra) const
{
    if (!ref.valid())
        return extra;
    const QVariantMap clip = m_controller->mcpCompactClip(ref.track, ref.clip);
    QJsonObject out = extra;
    out.insert(QStringLiteral("id"), clip.value(QStringLiteral("id")).toString());
    out.insert(QStringLiteral("track"), ref.track);
    out.insert(QStringLiteral("index"), ref.clip);
    auto copyNum = [&](const char *src, const char *dst) {
        if (clip.contains(QLatin1String(src)))
            out.insert(QString::fromUtf8(dst), clip.value(QLatin1String(src)).toDouble());
    };
    copyNum("start", "start");
    copyNum("duration", "dur");
    copyNum("inPoint", "in");
    copyNum("outPoint", "out");
    if (clip.contains(QStringLiteral("x")))
        out.insert(QStringLiteral("x"), clip.value(QStringLiteral("x")).toDouble());
    if (clip.contains(QStringLiteral("y")))
        out.insert(QStringLiteral("y"), clip.value(QStringLiteral("y")).toDouble());
    if (clip.contains(QStringLiteral("w")))
        out.insert(QStringLiteral("w"), clip.value(QStringLiteral("w")).toDouble());
    if (clip.contains(QStringLiteral("h")))
        out.insert(QStringLiteral("h"), clip.value(QStringLiteral("h")).toDouble());
    return out;
}

void McpDispatcher::moveClipToRequested(const ClipRef &ref, double at)
{
    if (!ref.valid() || !m_controller->allowClipOverlap())
        return;
    const QVariantMap clip = m_controller->mcpCompactClip(ref.track, ref.clip);
    if (qAbs(clip.value(QStringLiteral("start")).toDouble() - at) <= 0.001)
        return;
    m_controller->selectClip(ref.track, ref.clip);
    m_controller->moveClip(ref.track, ref.clip, at);
}

QJsonObject McpDispatcher::inspect(const QJsonObject &args) const
{
    return m_controller->mcpInspect(jsonBool(args.value(QStringLiteral("clips"))));
}

bool McpDispatcher::isUndoable(const QString &tool) const
{
    static const QStringList skip = {
        QStringLiteral("import_media"), QStringLiteral("list_assets"),
        QStringLiteral("list_effects"), QStringLiteral("list_audio_effects"),
        QStringLiteral("list_transitions"), QStringLiteral("seek"),
        QStringLiteral("play"),         QStringLiteral("pause"),
        QStringLiteral("undo"),         QStringLiteral("redo"),
        QStringLiteral("set_overlap"),
    };
    return !skip.contains(tool);
}

QJsonObject McpDispatcher::apply(const QJsonObject &args)
{
    const QJsonArray ops = args.value(QStringLiteral("ops")).toArray();
    if (ops.isEmpty())
        return err("bad_args", QStringLiteral("ops must be a non-empty array"));

    m_controller->mcpBeginBatch();
    bool hadUndoable = false;
    QJsonArray results;
    int i = 0;
    for (; i < ops.size(); ++i) {
        const QJsonObject op = ops.at(i).toObject();
        const QString tool = op.value(QStringLiteral("tool")).toString();
        const QJsonObject opArgs = op.value(QStringLiteral("args")).toObject();
        const QJsonObject one = applyOne(tool, opArgs);
        results.append(QJsonObject{{QStringLiteral("tool"), tool}, {QStringLiteral("result"), one}});
        if (!one.value(QStringLiteral("ok")).toBool()) {
            m_controller->mcpEndBatch(QStringLiteral("MCP apply, %1 ops").arg(i),
                                      hadUndoable && i > 0);
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("apply_failed")},
                    {QStringLiteral("stopped"), i},
                    {QStringLiteral("failed"), one},
                    {QStringLiteral("done"), results}};
        }
        if (isUndoable(tool))
            hadUndoable = true;
    }
    m_controller->mcpEndBatch(QStringLiteral("MCP apply, %1 ops").arg(ops.size()), hadUndoable);
    return ok({{QStringLiteral("n"), ops.size()}, {QStringLiteral("done"), results}});
}

QJsonObject McpDispatcher::applyOne(const QString &tool, const QJsonObject &args)
{
    if (tool == QLatin1String("import_media"))
        return opImportMedia(args);
    if (tool == QLatin1String("list_assets"))
        return opListAssets();
    if (tool == QLatin1String("rename_asset"))
        return opRenameAsset(args);
    if (tool == QLatin1String("add_track"))
        return opAddTrack(args);
    if (tool == QLatin1String("remove_track"))
        return opRemoveTrack(args);
    if (tool == QLatin1String("set_track"))
        return opSetTrack(args);
    if (tool == QLatin1String("place_clip"))
        return opPlaceClip(args);
    if (tool == QLatin1String("move_clip"))
        return opMoveClip(args);
    if (tool == QLatin1String("set_duration"))
        return opSetDuration(args);
    if (tool == QLatin1String("set_trim"))
        return opSetTrim(args);
    if (tool == QLatin1String("move_to_track"))
        return opMoveToTrack(args);
    if (tool == QLatin1String("split_clip"))
        return opSplitClip(args);
    if (tool == QLatin1String("delete_clip"))
        return opDeleteClip(args);
    if (tool == QLatin1String("duplicate_clip"))
        return opDuplicateClip(args);
    if (tool == QLatin1String("undo"))
        return opUndo();
    if (tool == QLatin1String("redo"))
        return opRedo();
    if (tool == QLatin1String("set_overlap"))
        return opSetOverlap(args);
    if (tool == QLatin1String("set_transform"))
        return opSetTransform(args);
    if (tool == QLatin1String("reset_transform"))
        return opResetTransform(args);
    if (tool == QLatin1String("seek"))
        return opSeek(args);
    if (tool == QLatin1String("play"))
        return opPlay();
    if (tool == QLatin1String("pause"))
        return opPause();
    if (tool == QLatin1String("set_work_area"))
        return opSetWorkArea(args);
    if (tool == QLatin1String("clear_work_area"))
        return opClearWorkArea();
    if (tool == QLatin1String("add_text"))
        return opAddText(args);
    if (tool == QLatin1String("set_text"))
        return opSetText(args);
    if (tool == QLatin1String("list_effects"))
        return opListEffects();
    if (tool == QLatin1String("list_audio_effects"))
        return opListAudioEffects();
    if (tool == QLatin1String("list_transitions"))
        return opListTransitions();
    if (tool == QLatin1String("add_effect"))
        return opAddEffect(args);
    if (tool == QLatin1String("remove_effect"))
        return opRemoveEffect(args);
    if (tool == QLatin1String("set_effect_param"))
        return opSetEffectParam(args);
    if (tool == QLatin1String("add_audio_effect"))
        return opAddAudioEffect(args);
    if (tool == QLatin1String("remove_audio_effect"))
        return opRemoveAudioEffect(args);
    if (tool == QLatin1String("set_audio_effect_param"))
        return opSetAudioEffectParam(args);
    if (tool == QLatin1String("add_transition"))
        return opAddTransition(args);
    if (tool == QLatin1String("remove_transition"))
        return opRemoveTransition(args);
    return err("unknown_op", tool);
}

QJsonObject McpDispatcher::capture(const QJsonObject &args)
{
    const double at = args.contains(QStringLiteral("at"))
                          ? jsonNumber(args.value(QStringLiteral("at")), -1.0)
                          : -1.0;
    const bool full = jsonBool(args.value(QStringLiteral("full")));
    return m_controller->mcpCaptureFrame(at, full);
}

QJsonObject McpDispatcher::waitImport(const QStringList &ids)
{
    AssetLibrary *lib = m_controller->assetLibrary();
    if (!lib)
        return err("not_found", QStringLiteral("No media bin"));

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(15000);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(lib, &AssetLibrary::assetMetadataChanged, &loop, &QEventLoop::quit);

    while (timeout.isActive()) {
        bool pending = false;
        for (const QString &id : ids) {
            if (lib->isImportPending(id)) {
                pending = true;
                break;
            }
        }
        if (!pending)
            break;
        loop.exec();
    }

    QJsonArray assets;
    bool anyTimeout = false;
    for (const QString &id : ids) {
        if (lib->isImportPending(id))
            anyTimeout = true;
        const int index = lib->indexOfId(id);
        const QVariantMap a = lib->assetAt(index);
        assets.append(QJsonObject{
            {QStringLiteral("id"), id},
            {QStringLiteral("index"), index},
            {QStringLiteral("name"), a.value(QStringLiteral("name")).toString()},
            {QStringLiteral("kind"), a.value(QStringLiteral("kind")).toString()},
            {QStringLiteral("dur"), a.value(QStringLiteral("durationSeconds")).toDouble()},
            {QStringLiteral("pending"), lib->isImportPending(id)},
        });
    }
    if (anyTimeout) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("import_timeout")},
                {QStringLiteral("detail"), QStringLiteral("Probe still running")},
                {QStringLiteral("assets"), assets}};
    }
    return ok({{QStringLiteral("assets"), assets}});
}

QJsonObject McpDispatcher::opImportMedia(const QJsonObject &args)
{
    AssetLibrary *lib = m_controller->assetLibrary();
    if (!lib)
        return err("not_found", QStringLiteral("No media bin"));
    const QStringList paths = jsonStringList(args.value(QStringLiteral("paths")));
    if (paths.isEmpty())
        return err("bad_args", QStringLiteral("paths required"));

    QStringList missing;
    QStringList existing;
    for (const QString &path : paths) {
        if (!QFileInfo::exists(path) || !QFileInfo(path).isFile())
            missing.append(path);
        else if (lib->indexOfPath(path) >= 0)
            existing.append(path);
    }
    const QStringList ids = lib->importLocalPaths(paths);
    if (ids.isEmpty() && !missing.isEmpty())
        return err("import_failed", QStringLiteral("No readable files"));
    QJsonObject result = waitImport(ids);
    if (!missing.isEmpty())
        result.insert(QStringLiteral("missing"), QJsonArray::fromStringList(missing));
    if (!existing.isEmpty())
        result.insert(QStringLiteral("refreshed"), QJsonArray::fromStringList(existing));
    return result;
}

QJsonObject McpDispatcher::opListAssets() const
{
    AssetLibrary *lib = m_controller->assetLibrary();
    QJsonArray assets;
    if (lib) {
        for (int i = 0; i < lib->count(); ++i) {
            const QVariantMap a = lib->assetAt(i);
            assets.append(QJsonObject{
                {QStringLiteral("index"), i},
                {QStringLiteral("id"), a.value(QStringLiteral("id")).toString()},
                {QStringLiteral("name"), a.value(QStringLiteral("name")).toString()},
                {QStringLiteral("kind"), a.value(QStringLiteral("kind")).toString()},
                {QStringLiteral("dur"), a.value(QStringLiteral("durationSeconds")).toDouble()},
                {QStringLiteral("w"), a.value(QStringLiteral("width")).toInt()},
                {QStringLiteral("h"), a.value(QStringLiteral("height")).toInt()},
            });
        }
    }
    return ok({{QStringLiteral("assets"), assets}});
}

QJsonObject McpDispatcher::opRenameAsset(const QJsonObject &args)
{
    const int index = resolveAsset(args.value(QStringLiteral("asset")));
    const QString name = args.value(QStringLiteral("name")).toString().trimmed();
    if (index < 0)
        return err("not_found", QStringLiteral("Unknown asset"));
    if (name.isEmpty())
        return err("bad_args", QStringLiteral("name required"));
    if (!m_controller->renameAsset(index, name))
        return err("bad_args", QStringLiteral("Rename refused"));
    return ok({{QStringLiteral("index"), index}, {QStringLiteral("name"), name}});
}

QJsonObject McpDispatcher::opAddTrack(const QJsonObject &args)
{
    const QString type = args.value(QStringLiteral("type")).toString().trimmed().toLower();
    const int before = m_controller->tracks().size();
    m_controller->addTrack(type);
    if (m_controller->tracks().size() == before)
        return err("bad_args", QStringLiteral("type must be video|audio|text|subtitle|shape"));
    return ok({{QStringLiteral("track"), 0}, {QStringLiteral("type"), type}});
}

QJsonObject McpDispatcher::opRemoveTrack(const QJsonObject &args)
{
    const int track = jsonInt(args.value(QStringLiteral("track")));
    if (track < 0 || track >= m_controller->tracks().size())
        return err("not_found", QStringLiteral("No such track"));
    m_controller->removeTrack(track);
    return ok({{QStringLiteral("removed"), track}});
}

QJsonObject McpDispatcher::opSetTrack(const QJsonObject &args)
{
    const int track = jsonInt(args.value(QStringLiteral("track")));
    if (track < 0 || track >= m_controller->tracks().size())
        return err("not_found", QStringLiteral("No such track"));
    if (args.contains(QStringLiteral("muted")))
        m_controller->setTrackMuted(track, jsonBool(args.value(QStringLiteral("muted"))));
    if (args.contains(QStringLiteral("hidden")))
        m_controller->setTrackHidden(track, jsonBool(args.value(QStringLiteral("hidden"))));
    return ok({{QStringLiteral("track"), track},
               {QStringLiteral("muted"), m_controller->trackMuted(track)},
               {QStringLiteral("hidden"), m_controller->trackHidden(track)}});
}

QJsonObject McpDispatcher::opPlaceClip(const QJsonObject &args)
{
    const int asset = resolveAsset(args.value(QStringLiteral("asset")));
    if (asset < 0)
        return err("not_found", QStringLiteral("Unknown asset"));
    const double at = args.contains(QStringLiteral("at"))
                          ? jsonNumber(args.value(QStringLiteral("at")), m_controller->playheadSeconds())
                          : m_controller->playheadSeconds();
    const QSet<QString> before = clipIdSet(m_controller);
    if (jsonBool(args.value(QStringLiteral("new_track")))) {
        const int insert = jsonInt(args.value(QStringLiteral("track")), 0);
        m_controller->addClipFromAssetOnNewTrackAt(asset, qMax(0, insert), at);
    } else if (args.contains(QStringLiteral("track"))) {
        const int track = jsonInt(args.value(QStringLiteral("track")));
        if (!m_controller->trackAcceptsAsset(track, asset))
            return err("type_mismatch", QStringLiteral("Track does not accept this asset"));
        m_controller->addClipFromAssetAt(asset, track, at);
    } else {
        m_controller->addClipFromAssetAt(
            asset,
            [&] {
                // Default track for this asset kind; fall back to addClipFromAsset path via index  lookup.
                for (int t = 0; t < m_controller->tracks().size(); ++t) {
                    if (m_controller->trackAcceptsAsset(t, asset))
                        return t;
                }
                return 0;
            }(),
            at);
        if (clipIdSet(m_controller) == before)
            m_controller->addClipFromAssetOnNewTrackAt(asset, 0, at);
    }

    const QSet<QString> after = clipIdSet(m_controller);
    QString newId;
    for (const QString &id : after) {
        if (!before.contains(id))
            newId = id;
    }
    if (newId.isEmpty())
        return err("type_mismatch", QStringLiteral("Clip was not placed"));
    const QPair<int, int> loc = m_controller->mcpLocateClip(newId);
    ClipRef ref{loc.first, loc.second, newId};
    moveClipToRequested(ref, at);
    const ClipRef placedRef = resolveClip(QJsonObject{{QStringLiteral("clip"), newId}});
    const QVariantMap clip = m_controller->mcpCompactClip(placedRef.track, placedRef.clip);
    const double placed = clip.value(QStringLiteral("start")).toDouble();
    QJsonObject extra{{QStringLiteral("requested"), at}};
    if (qAbs(placed - at) > 0.001)
        extra.insert(QStringLiteral("reason"), QStringLiteral("gap"));
    extra.insert(QStringLiteral("placed"), placed);
    extra.insert(QStringLiteral("asset"), asset);
    return ok(clipFeedback(placedRef, extra));
}

QJsonObject McpDispatcher::opMoveClip(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    if (!args.contains(QStringLiteral("at")))
        return err("bad_args", QStringLiteral("at required"));
    const double requested = jsonNumber(args.value(QStringLiteral("at")), 0);
    if (m_controller->allowClipOverlap()) {
        m_controller->selectClip(ref.track, ref.clip);
        m_controller->moveClip(ref.track, ref.clip, requested);
    } else {
        m_controller->setClipStart(ref.track, ref.clip, requested);
    }
    const ClipRef after = resolveClip(QJsonObject{{QStringLiteral("clip"), ref.id}});
    const QVariantMap clip = m_controller->mcpCompactClip(after.track, after.clip);
    const double placed = clip.value(QStringLiteral("start")).toDouble();
    QJsonObject extra{{QStringLiteral("requested"), requested}, {QStringLiteral("placed"), placed}};
    if (qAbs(placed - requested) > 0.001)
        extra.insert(QStringLiteral("reason"), QStringLiteral("gap"));
    return ok(clipFeedback(after, extra));
}

QJsonObject McpDispatcher::opSetDuration(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    const double requested = jsonNumber(args.value(QStringLiteral("duration")), -1);
    if (requested <= 0)
        return err("bad_args", QStringLiteral("duration must be > 0"));
    m_controller->setClipDuration(ref.track, ref.clip, requested);
    const ClipRef after = resolveClip(QJsonObject{{QStringLiteral("clip"), ref.id}});
    const QVariantMap clip = m_controller->mcpCompactClip(after.track, after.clip);
    QJsonObject extra{{QStringLiteral("requested"), requested},
                      {QStringLiteral("dur"), clip.value(QStringLiteral("duration")).toDouble()}};
    return ok(clipFeedback(after, extra));
}

QJsonObject McpDispatcher::opSetTrim(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    m_controller->setClipTrim(ref.track, ref.clip, jsonNumber(args.value(QStringLiteral("in")), 0),
                              jsonNumber(args.value(QStringLiteral("out")), 0));
    const ClipRef after = resolveClip(QJsonObject{{QStringLiteral("clip"), ref.id}});
    return ok(clipFeedback(after));
}

QJsonObject McpDispatcher::opMoveToTrack(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    const int toTrack = jsonInt(args.value(QStringLiteral("to_track")));
    if (toTrack < 0 || toTrack >= m_controller->tracks().size())
        return err("not_found", QStringLiteral("No such track"));
    const QVariantMap before = m_controller->mcpCompactClip(ref.track, ref.clip);
    const double at = args.contains(QStringLiteral("at"))
                          ? jsonNumber(args.value(QStringLiteral("at")), before.value(QStringLiteral("start")).toDouble())
                          : before.value(QStringLiteral("start")).toDouble();
    m_controller->moveClipToTrack(ref.track, ref.clip, toTrack, at);
    const ClipRef after = resolveClip(QJsonObject{{QStringLiteral("clip"), ref.id}});
    if (!after.valid())
        return err("type_mismatch", QStringLiteral("Move refused (type or locked)"));
    moveClipToRequested(after, at);
    const ClipRef placed = resolveClip(QJsonObject{{QStringLiteral("clip"), ref.id}});
    return ok(clipFeedback(placed));
}

QJsonObject McpDispatcher::opSplitClip(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    const double at = args.contains(QStringLiteral("at"))
                          ? jsonNumber(args.value(QStringLiteral("at")), m_controller->playheadSeconds())
                          : m_controller->playheadSeconds();
    const QSet<QString> before = clipIdSet(m_controller);
    m_controller->splitClipAt(ref.track, ref.clip, at);
    const QSet<QString> afterIds = clipIdSet(m_controller);
    QJsonArray ids;
    ids.append(ref.id);
    for (const QString &id : afterIds) {
        if (!before.contains(id))
            ids.append(id);
    }
    if (ids.size() < 2)
        return err("bad_args", QStringLiteral("Split needs a time inside the clip"));
    return ok({{QStringLiteral("clips"), ids}, {QStringLiteral("at"), at}});
}

QJsonObject McpDispatcher::opDeleteClip(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    m_controller->selectClip(ref.track, ref.clip);
    m_controller->deleteSelectedClip();
    if (m_controller->mcpLocateClip(ref.id).first >= 0)
        return err("bad_args", QStringLiteral("Delete refused"));
    return ok({{QStringLiteral("removed"), ref.id}});
}

QJsonObject McpDispatcher::opDuplicateClip(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    const QSet<QString> before = clipIdSet(m_controller);
    m_controller->selectClip(ref.track, ref.clip);
    m_controller->duplicateSelectedClip();
    QString newId;
    for (const QString &id : clipIdSet(m_controller)) {
        if (!before.contains(id))
            newId = id;
    }
    if (newId.isEmpty())
        return err("bad_args", QStringLiteral("Duplicate refused"));
    const QPair<int, int> loc = m_controller->mcpLocateClip(newId);
    return ok(clipFeedback({loc.first, loc.second, newId}));
}

QJsonObject McpDispatcher::opUndo()
{
    if (!m_controller->undoAvailable())
        return err("bad_args", QStringLiteral("Nothing to undo"));
    m_controller->undo();
    return ok({});
}

QJsonObject McpDispatcher::opRedo()
{
    if (!m_controller->redoAvailable())
        return err("bad_args", QStringLiteral("Nothing to redo"));
    m_controller->redo();
    return ok({});
}

QJsonObject McpDispatcher::opSetOverlap(const QJsonObject &args)
{
    if (!args.contains(QStringLiteral("enabled")))
        return err("bad_args", QStringLiteral("enabled required"));
    m_controller->setAllowClipOverlap(jsonBool(args.value(QStringLiteral("enabled"))));
    return ok({{QStringLiteral("overlap"), m_controller->allowClipOverlap()}});
}

QJsonObject McpDispatcher::opSetTransform(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    QVariantMap patch;
    for (const char *key : {"x", "y", "w", "h", "rotation", "opacity"}) {
        const QString k = QString::fromUtf8(key);
        if (args.contains(k))
            patch.insert(k, jsonNumber(args.value(k), 0));
    }
    if (patch.isEmpty())
        return err("bad_args", QStringLiteral("No transform fields"));
    if (!m_controller->mcpSetClipCanvas(ref.track, ref.clip, patch))
        return err("bad_args", QStringLiteral("Transform refused"));
    const ClipRef after = resolveClip(QJsonObject{{QStringLiteral("clip"), ref.id}});
    return ok(clipFeedback(after));
}

QJsonObject McpDispatcher::opResetTransform(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    m_controller->resetClipTransform(ref.track, ref.clip);
    const ClipRef after = resolveClip(QJsonObject{{QStringLiteral("clip"), ref.id}});
    return ok(clipFeedback(after));
}

QJsonObject McpDispatcher::opSeek(const QJsonObject &args)
{
    if (!args.contains(QStringLiteral("at")))
        return err("bad_args", QStringLiteral("at required"));
    m_controller->setPlayheadSeconds(jsonNumber(args.value(QStringLiteral("at")), 0));
    return ok({{QStringLiteral("at"), m_controller->playheadSeconds()}});
}

QJsonObject McpDispatcher::opPlay()
{
    m_controller->setPlaying(true);
    return ok({{QStringLiteral("playing"), m_controller->playing()}});
}

QJsonObject McpDispatcher::opPause()
{
    m_controller->setPlaying(false);
    return ok({{QStringLiteral("playing"), m_controller->playing()}});
}

QJsonObject McpDispatcher::opSetWorkArea(const QJsonObject &args)
{
    if (!args.contains(QStringLiteral("in")) || !args.contains(QStringLiteral("out")))
        return err("bad_args", QStringLiteral("in and out required"));
    const double inSeconds = jsonNumber(args.value(QStringLiteral("in")), -1);
    const double outSeconds = jsonNumber(args.value(QStringLiteral("out")), -1);
    if (!m_controller->mcpSetWorkArea(inSeconds, outSeconds))
        return err("bad_args", QStringLiteral("out must be greater than in"));
    return ok({{QStringLiteral("work_in"), m_controller->workAreaInSeconds()},
               {QStringLiteral("work_out"), m_controller->workAreaOutSeconds()}});
}

QJsonObject McpDispatcher::opClearWorkArea()
{
    m_controller->clearWorkArea();
    return ok({{QStringLiteral("cleared"), true}});
}

QJsonObject McpDispatcher::opAddText(const QJsonObject &args)
{
    QString text = args.value(QStringLiteral("text")).toString();
    if (text.trimmed().isEmpty())
        text = QStringLiteral("Text");
    const double at = args.contains(QStringLiteral("at"))
                          ? jsonNumber(args.value(QStringLiteral("at")), m_controller->playheadSeconds())
                          : m_controller->playheadSeconds();
    const QSet<QString> before = clipIdSet(m_controller);
    m_controller->addTextClip(text, at, args.value(QStringLiteral("preset")).toString());
    QString newId;
    for (const QString &id : clipIdSet(m_controller)) {
        if (!before.contains(id))
            newId = id;
    }
    if (newId.isEmpty())
        return err("bad_args", QStringLiteral("Text clip not added"));
    const QPair<int, int> loc = m_controller->mcpLocateClip(newId);
    const ClipRef ref{loc.first, loc.second, newId};
    moveClipToRequested(ref, at);
    const ClipRef after = resolveClip(QJsonObject{{QStringLiteral("clip"), newId}});
    return ok(clipFeedback(after));
}

QJsonObject McpDispatcher::opSetText(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    if (args.contains(QStringLiteral("text")))
        m_controller->setClipTextContent(ref.track, ref.clip, args.value(QStringLiteral("text")).toString());
    if (args.contains(QStringLiteral("style")))
        m_controller->setTextStyle(ref.track, ref.clip, args.value(QStringLiteral("style")).toObject().toVariantMap());
    const ClipRef after = resolveClip(QJsonObject{{QStringLiteral("clip"), ref.id}});
    return ok(clipFeedback(after));
}

QJsonObject McpDispatcher::opListEffects() const
{
    QJsonArray items;
    for (const QVariant &v : m_controller->effectCatalog())
        items.append(compactCatalogItem(v.toMap()));
    return ok({{QStringLiteral("effects"), items}});
}

QJsonObject McpDispatcher::opListAudioEffects() const
{
    QJsonArray items;
    for (const QVariant &v : m_controller->audioEffectCatalog())
        items.append(compactCatalogItem(v.toMap()));
    return ok({{QStringLiteral("effects"), items}});
}

QJsonObject McpDispatcher::opListTransitions() const
{
    QJsonArray items;
    for (const QVariant &v : m_controller->transitionKinds())
        items.append(compactCatalogItem(v.toMap()));
    return ok({{QStringLiteral("transitions"), items}});
}

QJsonObject McpDispatcher::opAddEffect(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    const QString effect = args.value(QStringLiteral("effect")).toString();
    const QVariantList before = m_controller->clipAt(ref.track, ref.clip).value(QStringLiteral("effects")).toList();
    m_controller->addEffect(ref.track, ref.clip, effect);
    const QVariantList after = m_controller->clipAt(ref.track, ref.clip).value(QStringLiteral("effects")).toList();
    if (after.size() <= before.size())
        return err("not_found", QStringLiteral("Unknown effect id"));
    return ok(clipFeedback(ref, {{QStringLiteral("index"), after.size() - 1},
                                 {QStringLiteral("effect"), effect}}));
}

QJsonObject McpDispatcher::opRemoveEffect(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    const int index = jsonInt(args.value(QStringLiteral("index")));
    const int before = m_controller->clipAt(ref.track, ref.clip).value(QStringLiteral("effects")).toList().size();
    m_controller->removeEffect(ref.track, ref.clip, index);
    const int after = m_controller->clipAt(ref.track, ref.clip).value(QStringLiteral("effects")).toList().size();
    if (after >= before)
        return err("not_found", QStringLiteral("No such effect index"));
    return ok(clipFeedback(ref, {{QStringLiteral("removed"), index}}));
}

QJsonObject McpDispatcher::opSetEffectParam(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    m_controller->setEffectParam(ref.track, ref.clip, jsonInt(args.value(QStringLiteral("index"))),
                                 args.value(QStringLiteral("key")).toString(),
                                 jsonNumber(args.value(QStringLiteral("value")), 0));
    return ok(clipFeedback(ref));
}

QJsonObject McpDispatcher::opAddAudioEffect(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    const QString effect = args.value(QStringLiteral("effect")).toString();
    const int before =
        m_controller->clipAt(ref.track, ref.clip).value(QStringLiteral("audioEffects")).toList().size();
    m_controller->addAudioEffect(ref.track, ref.clip, effect);
    const int after =
        m_controller->clipAt(ref.track, ref.clip).value(QStringLiteral("audioEffects")).toList().size();
    if (after <= before)
        return err("not_found", QStringLiteral("Unknown audio effect id"));
    return ok(clipFeedback(ref, {{QStringLiteral("index"), after - 1}, {QStringLiteral("effect"), effect}}));
}

QJsonObject McpDispatcher::opRemoveAudioEffect(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    const int index = jsonInt(args.value(QStringLiteral("index")));
    const int before =
        m_controller->clipAt(ref.track, ref.clip).value(QStringLiteral("audioEffects")).toList().size();
    m_controller->removeAudioEffect(ref.track, ref.clip, index);
    const int after =
        m_controller->clipAt(ref.track, ref.clip).value(QStringLiteral("audioEffects")).toList().size();
    if (after >= before)
        return err("not_found", QStringLiteral("No such audio effect index"));
    return ok(clipFeedback(ref, {{QStringLiteral("removed"), index}}));
}

QJsonObject McpDispatcher::opSetAudioEffectParam(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    m_controller->setAudioEffectParam(ref.track, ref.clip, jsonInt(args.value(QStringLiteral("index"))),
                                      args.value(QStringLiteral("key")).toString(),
                                      jsonNumber(args.value(QStringLiteral("value")), 0));
    return ok(clipFeedback(ref));
}

QJsonObject McpDispatcher::opAddTransition(const QJsonObject &args)
{
    const ClipRef ref = resolveClip(args);
    if (!ref.valid())
        return err("not_found", QStringLiteral("Unknown clip"));
    const QString kind = args.value(QStringLiteral("kind")).toString();
    const double duration = args.contains(QStringLiteral("duration"))
                                ? jsonNumber(args.value(QStringLiteral("duration")), 0.5)
                                : 0.5;
    m_controller->addTransition(ref.track, ref.clip, kind.isEmpty() ? QStringLiteral("crossfade") : kind,
                                duration);
    const QVariantMap tr = m_controller->transitionBetweenClips(ref.track, ref.clip);
    if (tr.isEmpty())
        return err("bad_args", QStringLiteral("No neighbour clip for a transition"));
    return ok({{QStringLiteral("id"), tr.value(QStringLiteral("id")).toString()},
               {QStringLiteral("kind"), tr.value(QStringLiteral("kind")).toString()},
               {QStringLiteral("dur"), tr.value(QStringLiteral("duration")).toDouble()},
               {QStringLiteral("track"), ref.track}});
}

QJsonObject McpDispatcher::opRemoveTransition(const QJsonObject &args)
{
    const int track = jsonInt(args.value(QStringLiteral("track")));
    const QString id = args.value(QStringLiteral("id")).toString();
    if (track < 0 || id.isEmpty())
        return err("bad_args", QStringLiteral("track and id required"));
    m_controller->removeTransition(track, id);
    return ok({{QStringLiteral("removed"), id}});
}

} // namespace drift::mcp

