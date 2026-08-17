#include <QtTest>

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QScopeGuard>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include "mcp/McpCatalog.h"
#include "mcp/McpDispatcher.h"
#include "mcp/McpProtocol.h"
#include "mcp/McpSession.h"
#include "models/AppController.h"
#include "models/AssetLibrary.h"

class McpTest : public QObject
{
    Q_OBJECT

private slots:
    void catalogListsToolboxes();
    void catalogOpsIncludeWhen();
    void toolboxDescriptionsIncludeWhen();
    void toolboxAnnotationsPresent();
    void toolboxUnknownIsError();
    void toolboxReturnsSchemas();
    void protocolInitializeAndToolsList();
    void protocolUnknownOp();
    void sessionFileRoundTrip();
    void sessionFileMissing();
    void serverRequiresBearerToken();
    void serverInitializeWithToken();
    void applyUnknownOp();
    void applyBatchStopsAndUndoRevertsPrefix();
    void inspectIsCompact();
    void inspectIncludesProjectFields();
    void placeHonorsOverlapToggle();
    void workAreaRoundTrip();
    void exportOptionsAndSettings();
    void exportVideoRequiresPath();
    void projectSetupRoundTrip();
    void captureDoesNotInsertClip();
    void inspectRevisionUnchanged();
    void listEffectsIncludesParams();
};

static QJsonObject rpc(const QString &method, const QJsonObject &params = {}, int id = 1)
{
    QJsonObject o{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("method"), method},
    };
    if (!params.isEmpty())
        o.insert(QStringLiteral("params"), params);
    return o;
}

static QByteArray httpPost(quint16 port, const QByteArray &auth, const QByteArray &body,
                           int *statusOut = nullptr)
{
    QTcpSocket socket;
    QEventLoop loop;
    QObject::connect(&socket, &QTcpSocket::connected, &loop, &QEventLoop::quit);
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    socket.connectToHost(QStringLiteral("127.0.0.1"), port);
    if (socket.state() != QAbstractSocket::ConnectedState)
        loop.exec();
    if (socket.state() != QAbstractSocket::ConnectedState)
        return {};

    QByteArray req = "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n";
    if (!auth.isEmpty()) {
        req += "Authorization: ";
        req += auth;
        req += "\r\n";
    }
    req += "Content-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n";
    req += body;
    socket.write(req);

    QByteArray response;
    QObject::connect(&socket, &QTcpSocket::readyRead, &loop, [&]() {
        response += socket.readAll();
        if (response.contains("\r\n\r\n")) {
            const int sep = response.indexOf("\r\n\r\n");
            const QByteArray header = response.left(sep);
            const int length = [&] {
                const QByteArray lower = header.toLower();
                const int at = lower.indexOf("content-length:");
                if (at < 0)
                    return 0;
                return header.mid(at + 15).trimmed().toInt();
            }();
            if (response.size() >= sep + 4 + length)
                loop.quit();
        }
    });
    QObject::connect(&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    response += socket.readAll();

    const int sep = response.indexOf("\r\n\r\n");
    if (sep < 0)
        return {};
    if (statusOut) {
        const QByteArray line = response.left(response.indexOf('\n'));
        const auto parts = line.split(' ');
        *statusOut = parts.size() >= 2 ? parts.at(1).toInt() : 0;
    }
    return response.mid(sep + 4);
}

void McpTest::catalogListsToolboxes()
{
    const QJsonObject cat = drift::mcp::catalogPayload();
    QVERIFY(cat.value(QStringLiteral("ok")).toBool());
    const QJsonArray boxes = cat.value(QStringLiteral("toolboxes")).toArray();
    QCOMPARE(boxes.size(), 10);
    QStringList names;
    for (const QJsonValue &v : boxes)
        names.append(v.toObject().value(QStringLiteral("name")).toString());
    QVERIFY(names.contains(QStringLiteral("media")));
    QVERIFY(names.contains(QStringLiteral("timeline")));
    QVERIFY(names.contains(QStringLiteral("canvas")));
    QVERIFY(names.contains(QStringLiteral("project")));
}

void McpTest::catalogOpsIncludeWhen()
{
    const QJsonObject cat = drift::mcp::catalogPayload();
    const QJsonArray boxes = cat.value(QStringLiteral("toolboxes")).toArray();
    QVERIFY(cat.contains(QStringLiteral("endpoints")));
    QVERIFY(cat.contains(QStringLiteral("units")));
    QVERIFY(cat.contains(QStringLiteral("workflow")));
    QVERIFY(cat.contains(QStringLiteral("guide")));
    for (const QJsonValue &v : boxes) {
        const QJsonArray ops = v.toObject().value(QStringLiteral("ops")).toArray();
        QVERIFY(!ops.isEmpty());
        const QJsonObject first = ops.at(0).toObject();
        QVERIFY(first.contains(QStringLiteral("name")));
        QVERIFY(first.contains(QStringLiteral("when")));
    }
}

void McpTest::toolboxDescriptionsIncludeWhen()
{
    const QJsonObject payload = drift::mcp::toolboxPayload(QStringLiteral("timeline"));
    const QJsonArray tools = payload.value(QStringLiteral("tools")).toArray();
    bool sawAddTrack = false;
    for (const QJsonValue &v : tools) {
        const QJsonObject tool = v.toObject();
        if (tool.value(QStringLiteral("name")).toString() == QLatin1String("add_track")) {
            sawAddTrack = true;
            QVERIFY(tool.value(QStringLiteral("description")).toString().startsWith(QStringLiteral("When:")));
            const QJsonObject type =
                tool.value(QStringLiteral("inputSchema")).toObject()
                    .value(QStringLiteral("properties")).toObject()
                    .value(QStringLiteral("type")).toObject();
            QVERIFY(type.contains(QStringLiteral("enum")));
        }
    }
    QVERIFY(sawAddTrack);
}

void McpTest::toolboxAnnotationsPresent()
{
    const QJsonObject payload = drift::mcp::toolboxPayload(QStringLiteral("media"));
    const QJsonArray tools = payload.value(QStringLiteral("tools")).toArray();
    bool sawList = false;
    bool sawRename = false;
    for (const QJsonValue &v : tools) {
        const QJsonObject tool = v.toObject();
        const QString name = tool.value(QStringLiteral("name")).toString();
        if (name == QLatin1String("list_assets")) {
            sawList = true;
            const QJsonObject ann = tool.value(QStringLiteral("annotations")).toObject();
            QVERIFY(ann.value(QStringLiteral("readOnlyHint")).toBool());
            QVERIFY(ann.value(QStringLiteral("idempotentHint")).toBool());
        }
        if (name == QLatin1String("rename_asset"))
            sawRename = true;
    }
    QVERIFY(sawList);
    QVERIFY(sawRename);
    QVERIFY(drift::mcp::toolboxNames().contains(QStringLiteral("project")));
}

void McpTest::toolboxUnknownIsError()
{
    const QJsonObject payload = drift::mcp::toolboxPayload(QStringLiteral("nope"));
    QCOMPARE(payload.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(payload.value(QStringLiteral("error")).toString(), QStringLiteral("unknown_toolbox"));
}

void McpTest::toolboxReturnsSchemas()
{
    const QJsonObject payload = drift::mcp::toolboxPayload(QStringLiteral("timeline"));
    QVERIFY(payload.value(QStringLiteral("ok")).toBool());
    const QJsonArray tools = payload.value(QStringLiteral("tools")).toArray();
    QVERIFY(tools.size() >= 8);
    bool sawPlace = false;
    for (const QJsonValue &v : tools) {
        if (v.toObject().value(QStringLiteral("name")).toString() == QLatin1String("place_clip")) {
            sawPlace = true;
            QVERIFY(v.toObject().contains(QStringLiteral("inputSchema")));
        }
    }
    QVERIFY(sawPlace);
}

void McpTest::protocolInitializeAndToolsList()
{
    const QJsonValue init = drift::mcp::handleJsonRpc(rpc(QStringLiteral("initialize")), {}, {});
    QVERIFY(init.isObject());
    QCOMPARE(init.toObject().value(QStringLiteral("result")).toObject()
                 .value(QStringLiteral("serverInfo")).toObject()
                 .value(QStringLiteral("name")).toString(),
             QStringLiteral("drift"));

    const QJsonValue listed = drift::mcp::handleJsonRpc(rpc(QStringLiteral("tools/list")), {}, {});
    const QJsonArray tools =
        listed.toObject().value(QStringLiteral("result")).toObject().value(QStringLiteral("tools")).toArray();
    QCOMPARE(tools.size(), 5);
}

void McpTest::protocolUnknownOp()
{
    bool called = false;
    const QJsonValue reply = drift::mcp::handleJsonRpc(
        rpc(QStringLiteral("tools/call"),
            {{QStringLiteral("name"), QStringLiteral("not_a_tool")},
             {QStringLiteral("arguments"), QJsonObject{}}}),
        {},
        [&](const QString &, const QJsonObject &) {
            called = true;
            return QJsonObject{};
        });
    QVERIFY(!called);
    const QJsonArray content = reply.toObject()
                                   .value(QStringLiteral("result"))
                                   .toObject()
                                   .value(QStringLiteral("content"))
                                   .toArray();
    QVERIFY(!content.isEmpty());
    const auto payload = QJsonDocument::fromJson(
                             content.at(0).toObject().value(QStringLiteral("text")).toString().toUtf8())
                             .object();
    QCOMPARE(payload.value(QStringLiteral("error")).toString(), QStringLiteral("unknown_op"));
}

void McpTest::sessionFileRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("session.json"));
    qputenv("DRIFT_MCP_SESSION_PATH", path.toUtf8());
    QVERIFY(drift::mcp::writeSessionFile(4731, QStringLiteral("abc123")));
    quint16 port = 0;
    QString token;
    QString error;
    QVERIFY(drift::mcp::readSessionFile(&port, &token, &error));
    QCOMPARE(port, quint16(4731));
    QCOMPARE(token, QStringLiteral("abc123"));
    drift::mcp::removeSessionFile();
    QVERIFY(!QFile::exists(path));
    qunsetenv("DRIFT_MCP_SESSION_PATH");
}

void McpTest::sessionFileMissing()
{
    qputenv("DRIFT_MCP_SESSION_PATH", "/tmp/drift-mcp-does-not-exist-test.json");
    QString error;
    QVERIFY(!drift::mcp::readSessionFile(nullptr, nullptr, &error));
    QVERIFY(error.contains(QStringLiteral("Agent access")));
    qunsetenv("DRIFT_MCP_SESSION_PATH");
}

void McpTest::serverRequiresBearerToken()
{
    QTemporaryDir dir;
    qputenv("DRIFT_MCP_SESSION_PATH", dir.filePath(QStringLiteral("s.json")).toUtf8());
    AssetLibrary library;
    AppController state(&library);
    state.setMcpEnabled(true);
    QVERIFY2(state.mcpRunning(), qPrintable(state.mcpError()));
    int status = 0;
    httpPost(quint16(state.mcpPort()), {},
             QJsonDocument(rpc(QStringLiteral("ping"))).toJson(QJsonDocument::Compact), &status);
    QCOMPARE(status, 401);
    state.setMcpEnabled(false);
    QVERIFY(!state.mcpRunning());
    qunsetenv("DRIFT_MCP_SESSION_PATH");
}

void McpTest::serverInitializeWithToken()
{
    QTemporaryDir dir;
    qputenv("DRIFT_MCP_SESSION_PATH", dir.filePath(QStringLiteral("s.json")).toUtf8());
    AssetLibrary library;
    AppController state(&library);
    state.setMcpEnabled(true);
    QVERIFY(state.mcpRunning());
    int status = 0;
    const QByteArray auth = "Bearer " + state.mcpToken().toUtf8();
    const QByteArray body = httpPost(
        quint16(state.mcpPort()), auth,
        QJsonDocument(rpc(QStringLiteral("initialize"))).toJson(QJsonDocument::Compact), &status);
    QCOMPARE(status, 200);
    const auto doc = QJsonDocument::fromJson(body);
    QCOMPARE(doc.object().value(QStringLiteral("result")).toObject()
                 .value(QStringLiteral("serverInfo")).toObject()
                 .value(QStringLiteral("name")).toString(),
             QStringLiteral("drift"));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("s.json"))));
    state.setMcpEnabled(false);
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("s.json"))));
    qunsetenv("DRIFT_MCP_SESSION_PATH");
}

void McpTest::applyUnknownOp()
{
    AssetLibrary library;
    AppController state(&library);
    drift::mcp::McpDispatcher dispatcher(&state);
    const QJsonObject result =
        dispatcher.applyOne(QStringLiteral("not_real"), {});
    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("unknown_op"));
}

void McpTest::applyBatchStopsAndUndoRevertsPrefix()
{
    AssetLibrary library;
    AppController state(&library);
    state.addTextClip(QStringLiteral("Hello"), 0.0);
    const int track = state.selectedTrack();
    const int clip = state.selectedClip();
    QVERIFY(track >= 0);
    const QString id = state.mcpCompactClip(track, clip).value(QStringLiteral("id")).toString();
    QVERIFY(!id.isEmpty());

    drift::mcp::McpDispatcher dispatcher(&state);
    const QJsonObject batch = dispatcher.apply(QJsonObject{
        {QStringLiteral("ops"),
         QJsonArray{
             QJsonObject{{QStringLiteral("tool"), QStringLiteral("set_duration")},
                         {QStringLiteral("args"),
                          QJsonObject{{QStringLiteral("clip"), id},
                                      {QStringLiteral("duration"), 2.0}}}},
             QJsonObject{{QStringLiteral("tool"), QStringLiteral("not_real")},
                         {QStringLiteral("args"), QJsonObject{}}},
         }},
    });
    QCOMPARE(batch.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(batch.value(QStringLiteral("error")).toString(), QStringLiteral("apply_failed"));
    QCOMPARE(batch.value(QStringLiteral("stopped")).toInt(), 1);
    QCOMPARE(state.mcpCompactClip(track, clip).value(QStringLiteral("duration")).toDouble(), 2.0);

    QVERIFY(state.undoAvailable());
    state.undo();
    QVERIFY(state.mcpCompactClip(track, clip).value(QStringLiteral("duration")).toDouble() > 2.0);
}

void McpTest::inspectIsCompact()
{
    AssetLibrary library;
    AppController state(&library);
    state.addTextClip(QStringLiteral("Hello"), 0.0);
    drift::mcp::McpDispatcher dispatcher(&state);
    const QJsonObject summary = dispatcher.inspect({});
    QVERIFY(summary.value(QStringLiteral("ok")).toBool());
    QVERIFY(summary.contains(QStringLiteral("tracks")));
    QVERIFY(summary.contains(QStringLiteral("overlap")));
    QCOMPARE(summary.value(QStringLiteral("overlap")).toBool(), false);
    QVERIFY(!summary.contains(QStringLiteral("work_in")));
    QVERIFY(!summary.toVariantMap().contains(QStringLiteral("effects")));
    QCOMPARE(summary.value(QStringLiteral("path")).toString(), QString());
    QCOMPARE(summary.value(QStringLiteral("dirty")).toBool(), true);
    QVERIFY(summary.contains(QStringLiteral("background")));
    QVERIFY(summary.contains(QStringLiteral("export")));
    const QJsonObject withClips = dispatcher.inspect({{QStringLiteral("clips"), true}});
    const QJsonArray tracks = withClips.value(QStringLiteral("tracks")).toArray();
    QVERIFY(!tracks.isEmpty());
    QVERIFY(tracks.at(0).toObject().contains(QStringLiteral("items")));
}

void McpTest::inspectIncludesProjectFields()
{
    AssetLibrary library;
    AppController state(&library);
    drift::mcp::McpDispatcher dispatcher(&state);
    const QJsonObject inspect = dispatcher.inspect({});
    QVERIFY(inspect.contains(QStringLiteral("path")));
    QVERIFY(inspect.contains(QStringLiteral("dirty")));
    QVERIFY(inspect.contains(QStringLiteral("background")));
    const QJsonObject exportState = inspect.value(QStringLiteral("export")).toObject();
    QVERIFY(exportState.contains(QStringLiteral("active")));
    QVERIFY(exportState.contains(QStringLiteral("progress")));
}

void McpTest::placeHonorsOverlapToggle()
{
    AssetLibrary library;
    AppController state(&library);
    drift::mcp::McpDispatcher dispatcher(&state);

    const QJsonObject first = dispatcher.applyOne(
        QStringLiteral("add_text"),
        {{QStringLiteral("text"), QStringLiteral("A")}, {QStringLiteral("at"), 0.0}});
    QVERIFY(first.value(QStringLiteral("ok")).toBool());

    const QJsonObject gapped = dispatcher.applyOne(
        QStringLiteral("add_text"),
        {{QStringLiteral("text"), QStringLiteral("B")}, {QStringLiteral("at"), 1.0}});
    QVERIFY(gapped.value(QStringLiteral("ok")).toBool());
    QVERIFY(gapped.value(QStringLiteral("start")).toDouble() > 4.0);

    QVERIFY(state.undoAvailable());
    state.undo();

    const QJsonObject overlapOn = dispatcher.applyOne(
        QStringLiteral("set_overlap"), {{QStringLiteral("enabled"), true}});
    QVERIFY(overlapOn.value(QStringLiteral("ok")).toBool());
    QCOMPARE(overlapOn.value(QStringLiteral("overlap")).toBool(), true);

    const QJsonObject overlapping = dispatcher.applyOne(
        QStringLiteral("add_text"),
        {{QStringLiteral("text"), QStringLiteral("B")}, {QStringLiteral("at"), 1.0}});
    QVERIFY(overlapping.value(QStringLiteral("ok")).toBool());
    QCOMPARE(overlapping.value(QStringLiteral("start")).toDouble(), 1.0);
    QVERIFY(!overlapping.contains(QStringLiteral("reason")));
    QCOMPARE(dispatcher.inspect({}).value(QStringLiteral("overlap")).toBool(), true);
}

void McpTest::workAreaRoundTrip()
{
    AssetLibrary library;
    AppController state(&library);
    drift::mcp::McpDispatcher dispatcher(&state);

    const QJsonObject bad = dispatcher.applyOne(
        QStringLiteral("set_work_area"),
        {{QStringLiteral("in"), 5.0}, {QStringLiteral("out"), 1.0}});
    QCOMPARE(bad.value(QStringLiteral("ok")).toBool(), false);

    const QJsonObject set = dispatcher.applyOne(
        QStringLiteral("set_work_area"),
        {{QStringLiteral("in"), 1.5}, {QStringLiteral("out"), 4.0}});
    QVERIFY(set.value(QStringLiteral("ok")).toBool());
    QCOMPARE(set.value(QStringLiteral("work_in")).toDouble(), 1.5);
    QCOMPARE(set.value(QStringLiteral("work_out")).toDouble(), 4.0);

    const QJsonObject inspect = dispatcher.inspect({});
    QCOMPARE(inspect.value(QStringLiteral("work_in")).toDouble(), 1.5);
    QCOMPARE(inspect.value(QStringLiteral("work_out")).toDouble(), 4.0);

    const QJsonObject cleared = dispatcher.applyOne(QStringLiteral("clear_work_area"), {});
    QVERIFY(cleared.value(QStringLiteral("ok")).toBool());
    QVERIFY(!dispatcher.inspect({}).contains(QStringLiteral("work_in")));
}

void McpTest::exportOptionsAndSettings()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString org = QCoreApplication::organizationName();
    const QString app = QCoreApplication::applicationName();
    QCoreApplication::setOrganizationName(QStringLiteral("DriftMcpTest"));
    QCoreApplication::setApplicationName(QStringLiteral("DriftMcpTest"));
    const auto restore = qScopeGuard([&] {
        QSettings().remove(QStringLiteral("export"));
        QCoreApplication::setOrganizationName(org);
        QCoreApplication::setApplicationName(app);
        QStandardPaths::setTestModeEnabled(false);
    });
    QSettings().remove(QStringLiteral("export"));

    AssetLibrary library;
    AppController state(&library);
    drift::mcp::McpDispatcher dispatcher(&state);

    const QJsonObject options = dispatcher.applyOne(QStringLiteral("list_export_options"), {});
    QVERIFY(options.value(QStringLiteral("ok")).toBool());
    QVERIFY(!options.value(QStringLiteral("video")).toArray().isEmpty());
    QVERIFY(!options.value(QStringLiteral("fps")).toArray().isEmpty());
    QVERIFY(options.contains(QStringLiteral("gif")));
}

void McpTest::exportVideoRequiresPath()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString org = QCoreApplication::organizationName();
    const QString app = QCoreApplication::applicationName();
    QCoreApplication::setOrganizationName(QStringLiteral("DriftMcpTest"));
    QCoreApplication::setApplicationName(QStringLiteral("DriftMcpTest"));
    const auto restore = qScopeGuard([&] {
        QSettings().remove(QStringLiteral("export"));
        QCoreApplication::setOrganizationName(org);
        QCoreApplication::setApplicationName(app);
        QStandardPaths::setTestModeEnabled(false);
    });
    QSettings().remove(QStringLiteral("export"));

    AssetLibrary library;
    AppController state(&library);
    drift::mcp::McpDispatcher dispatcher(&state);

    const QJsonObject missing = dispatcher.applyOne(QStringLiteral("export_video"), {});
    QCOMPARE(missing.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(missing.value(QStringLiteral("error")).toString(), QStringLiteral("bad_args"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outPath = dir.filePath(QStringLiteral("out.mp4"));
    const QJsonObject started = dispatcher.applyOne(
        QStringLiteral("export_video"), {{QStringLiteral("path"), outPath}});
    if (started.value(QStringLiteral("ok")).toBool()) {
        QVERIFY(started.value(QStringLiteral("started")).toBool());
        const QJsonObject status = dispatcher.applyOne(QStringLiteral("export_status"), {});
        QVERIFY(status.contains(QStringLiteral("busy")));
        QVERIFY(status.contains(QStringLiteral("progress")));
    }
}

void McpTest::projectSetupRoundTrip()
{
    AssetLibrary library;
    AppController state(&library);
    drift::mcp::McpDispatcher dispatcher(&state);

    const QJsonObject setup = dispatcher.applyOne(
        QStringLiteral("set_project_setup"),
        {{QStringLiteral("width"), 1280}, {QStringLiteral("height"), 720}, {QStringLiteral("fps"), 30}});
    QVERIFY(setup.value(QStringLiteral("ok")).toBool());
    QCOMPARE(setup.value(QStringLiteral("w")).toInt(), 1280);
    QCOMPARE(setup.value(QStringLiteral("h")).toInt(), 720);
    QCOMPARE(setup.value(QStringLiteral("fps")).toInt(), 30);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("test.dcut"));
    const QJsonObject saved = dispatcher.applyOne(
        QStringLiteral("save_project"), {{QStringLiteral("path"), path}});
    QVERIFY(saved.value(QStringLiteral("ok")).toBool());
    QVERIFY(QFile::exists(path));
}

void McpTest::captureDoesNotInsertClip()
{
    AssetLibrary library;
    AppController state(&library);
    state.addTextClip(QStringLiteral("Hello"), 0.0);
    auto clipCount = [](AppController &s) {
        int n = 0;
        for (const QVariant &t : s.tracks())
            n += t.toMap().value(QStringLiteral("clips")).toList().size();
        return n;
    };
    const int before = clipCount(state);
    const QJsonObject result = state.mcpCaptureFrame(-1.0, false);
    QCOMPARE(clipCount(state), before);
    if (result.value(QStringLiteral("isError")).toBool())
        QSKIP("Compositor could not produce a frame in this environment");
    QVERIFY(result.value(QStringLiteral("content")).toArray().size() >= 1);
}

void McpTest::inspectRevisionUnchanged()
{
    AssetLibrary library;
    AppController state(&library);
    drift::mcp::McpDispatcher dispatcher(&state);
    const QJsonObject first = dispatcher.inspect({});
    QVERIFY(first.value(QStringLiteral("ok")).toBool());
    QVERIFY(first.contains(QStringLiteral("revision")));
    const int revision = first.value(QStringLiteral("revision")).toInt();
    const QJsonObject unchanged = dispatcher.inspect({{QStringLiteral("since"), revision}});
    QVERIFY(unchanged.value(QStringLiteral("ok")).toBool());
    QCOMPARE(unchanged.value(QStringLiteral("unchanged")).toBool(), true);
    QCOMPARE(unchanged.value(QStringLiteral("revision")).toInt(), revision);
}

void McpTest::listEffectsIncludesParams()
{
    AssetLibrary library;
    AppController state(&library);
    drift::mcp::McpDispatcher dispatcher(&state);
    const QJsonObject result = dispatcher.applyOne(QStringLiteral("list_effects"), {});
    QVERIFY(result.value(QStringLiteral("ok")).toBool());
    const QJsonArray effects = result.value(QStringLiteral("effects")).toArray();
    QVERIFY(!effects.isEmpty());
    bool sawParams = false;
    for (const QJsonValue &v : effects) {
        const QJsonArray params = v.toObject().value(QStringLiteral("params")).toArray();
        if (!params.isEmpty()) {
            sawParams = true;
            QVERIFY(params.at(0).toObject().contains(QStringLiteral("key")));
            break;
        }
    }
    QVERIFY(sawParams);
}

QTEST_MAIN(McpTest)
#include "tst_mcp.moc"
