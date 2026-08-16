#include "mcp/McpCatalog.h"
#include "mcp/McpJson.h"

namespace drift::mcp {
namespace {

QJsonObject clipRefProps()
{
    return {
        {QStringLiteral("clip"), stringProp(QStringLiteral("Clip UUID (preferred)"))},
        {QStringLiteral("track"), integerProp(QStringLiteral("Track index if clip id omitted (0 = top)"))},
        {QStringLiteral("index"), integerProp(QStringLiteral("Clip index on that track if clip id omitted"))},
    };
}

QJsonObject mergeProps(QJsonObject a, const QJsonObject &b)
{
    for (auto it = b.begin(); it != b.end(); ++it)
        a.insert(it.key(), it.value());
    return a;
}

struct Op {
    const char *name;
    const char *toolbox;
    const char *when;
    const char *description;
    QJsonObject schema;
};

const QList<Op> &ops()
{
    static const QList<Op> k = {
        { "import_media", "media", "Bring files into the bin",
          "Import local media files. Waits for probe. Duplicate paths refresh the existing row.",
          objectSchema({{QStringLiteral("paths"),
                         arrayProp({{QStringLiteral("type"), QStringLiteral("string")}},
                                   QStringLiteral("Absolute file paths"))}},
                       {QStringLiteral("paths")}) },
        { "list_assets", "media", "See what is in the bin",
          "List imported assets (id, name, kind, duration, size).",
          objectSchema({}) },
        { "rename_asset", "media", "Rename a bin row",
          "Rename an asset in the bin. Does not rename the file on disk.",
          objectSchema({{QStringLiteral("asset"), stringProp(QStringLiteral("Asset id or bin index as string"))},
                        {QStringLiteral("name"), stringProp(QStringLiteral("New display name"))}},
                       {QStringLiteral("asset"), QStringLiteral("name")}) },

        { "add_track", "timeline", "Need a new lane",
          "Prepend a track. type: video|audio|text|subtitle|shape. New track becomes index 0.",
          objectSchema({{QStringLiteral("type"), stringProp(QStringLiteral("video|audio|text|subtitle|shape"))}},
                       {QStringLiteral("type")}) },
        { "remove_track", "timeline", "Delete a lane and its clips",
          "Delete a track and everything on it.",
          objectSchema({{QStringLiteral("track"), integerProp(QStringLiteral("Track index"))}},
                       {QStringLiteral("track")}) },
        { "set_track", "timeline", "Mute or hide a lane",
          "Set track muted/hidden.",
          objectSchema(mergeProps({{QStringLiteral("track"), integerProp(QStringLiteral("Track index"))},
                                   {QStringLiteral("muted"), boolProp(QStringLiteral("Mute"))},
                                   {QStringLiteral("hidden"), boolProp(QStringLiteral("Hide from composite"))}},
                                  {}),
                       {QStringLiteral("track")}) },
        { "place_clip", "timeline", "Put media on the timeline",
          "Place an asset as a clip. When overlap is off (default), start may be pushed to the next gap.",
          objectSchema(mergeProps({{QStringLiteral("asset"), stringProp(QStringLiteral("Asset id or bin index as string"))},
                                   {QStringLiteral("at"), numberProp(QStringLiteral("Timeline start seconds (default: playhead)"))},
                                   {QStringLiteral("track"), integerProp(QStringLiteral("Destination track"))},
                                   {QStringLiteral("new_track"), boolProp(QStringLiteral("Insert a new matching track above"))}},
                                  {})) },
        { "move_clip", "timeline", "Change a clip's start time",
          "Move a clip on its track. When overlap is off (default), start may be pushed forward.",
          objectSchema(mergeProps({{QStringLiteral("at"), numberProp(QStringLiteral("New start seconds"))}},
                                  clipRefProps()),
                       {QStringLiteral("at")}) },
        { "set_duration", "timeline", "Change timeline length",
          "Set the clip's timeline duration in seconds. Trims source out (or in if reversed).",
          objectSchema(mergeProps({{QStringLiteral("duration"), numberProp(QStringLiteral("Seconds"))}},
                                  clipRefProps()),
                       {QStringLiteral("duration")}) },
        { "set_trim", "timeline", "Set source in/out",
          "Set source in/out points in seconds. Recomputes timeline duration from the span and speed.",
          objectSchema(mergeProps({{QStringLiteral("in"), numberProp(QStringLiteral("Source in seconds"))},
                                   {QStringLiteral("out"), numberProp(QStringLiteral("Source out seconds"))}},
                                  clipRefProps()),
                       {QStringLiteral("in"), QStringLiteral("out")}) },
        { "move_to_track", "timeline", "Move a clip to another lane",
          "Move a clip to another track. Type must match the destination.",
          objectSchema(mergeProps({{QStringLiteral("to_track"), integerProp(QStringLiteral("Destination track"))},
                                   {QStringLiteral("at"), numberProp(QStringLiteral("Start seconds (default: current)"))}},
                                  clipRefProps()),
                       {QStringLiteral("to_track")}) },
        { "split_clip", "timeline", "Cut a clip in two",
          "Split a clip at `at` seconds (default: playhead). Playhead must sit inside the clip.",
          objectSchema(mergeProps({{QStringLiteral("at"), numberProp(QStringLiteral("Timeline time seconds"))}},
                                  clipRefProps())) },
        { "delete_clip", "timeline", "Remove a clip",
          "Delete a clip (and its linked A/V partner).",
          objectSchema(clipRefProps()) },
        { "duplicate_clip", "timeline", "Copy a clip after itself",
          "Duplicate a clip immediately after it on the same track.",
          objectSchema(clipRefProps()) },
        { "undo", "timeline", "Revert the last edit",
          "Undo the last project edit.",
          objectSchema({}) },
        { "redo", "timeline", "Re-apply an undone edit",
          "Redo the last undone edit.",
          objectSchema({}) },
        { "set_overlap", "timeline", "Allow clips to overlap",
          "Project setting. Off (default): place/move push to the next gap. On: requested start is kept.",
          objectSchema({{QStringLiteral("enabled"), boolProp(QStringLiteral("Allow overlapping clips"))}},
                       {QStringLiteral("enabled")}) },

        { "set_transform", "canvas", "Position, size, rotate, fade a clip",
          "Set canvas transform in pixels. Omitted fields are left unchanged. x/y is top-left.",
          objectSchema(mergeProps({{QStringLiteral("x"), numberProp(QStringLiteral("Left, pixels"))},
                                   {QStringLiteral("y"), numberProp(QStringLiteral("Top, pixels"))},
                                   {QStringLiteral("w"), numberProp(QStringLiteral("Width, pixels"))},
                                   {QStringLiteral("h"), numberProp(QStringLiteral("Height, pixels"))},
                                   {QStringLiteral("rotation"), numberProp(QStringLiteral("Degrees clockwise"))},
                                   {QStringLiteral("opacity"), numberProp(QStringLiteral("0..1"))}},
                                  clipRefProps())) },
        { "reset_transform", "canvas", "Reset a clip to fill the canvas",
          "Reset position, size, rotation, opacity, and flips.",
          objectSchema(clipRefProps()) },

        { "seek", "playback", "Jump the playhead",
          "Seek the playhead to `at` seconds.",
          objectSchema({{QStringLiteral("at"), numberProp(QStringLiteral("Seconds"))}},
                       {QStringLiteral("at")}) },
        { "play", "playback", "Start playback",
          "Start playback from the current playhead.",
          objectSchema({}) },
        { "pause", "playback", "Stop playback",
          "Pause playback.",
          objectSchema({}) },
        { "set_work_area", "playback", "Set the In/Out range",
          "Set the In/Out work area in seconds. Used for looping and ranged export.",
          objectSchema({{QStringLiteral("in"), numberProp(QStringLiteral("In point seconds"))},
                        {QStringLiteral("out"), numberProp(QStringLiteral("Out point seconds"))}},
                       {QStringLiteral("in"), QStringLiteral("out")}) },
        { "clear_work_area", "playback", "Clear the In/Out range",
          "Clear the In/Out work area.",
          objectSchema({}) },

        { "add_text", "text", "Put a title or caption on the timeline",
          "Add a text clip. Empty text becomes \"Text\".",
          objectSchema({{QStringLiteral("text"), stringProp(QStringLiteral("Caption"))},
                        {QStringLiteral("at"), numberProp(QStringLiteral("Start seconds (default: playhead)"))},
                        {QStringLiteral("preset"), stringProp(QStringLiteral("Optional text style pack id"))}}) },
        { "set_text", "text", "Change caption copy or style",
          "Set text content and/or a partial style patch (fontFamily, pixelSize, color, align, …).",
          objectSchema(mergeProps({{QStringLiteral("text"), stringProp(QStringLiteral("New content"))},
                                   {QStringLiteral("style"),
                                    objectSchema({}, {})}},
                                  clipRefProps())) },

        { "list_effects", "effects", "See available video effects",
          "List video effect ids and labels.",
          objectSchema({}) },
        { "list_audio_effects", "effects", "See available audio effects",
          "List audio effect ids and labels.",
          objectSchema({}) },
        { "list_transitions", "effects", "See available transitions",
          "List transition ids and labels.",
          objectSchema({}) },
        { "add_effect", "effects", "Put a video effect on a clip",
          "Append a video effect to a clip.",
          objectSchema(mergeProps({{QStringLiteral("effect"), stringProp(QStringLiteral("Effect catalog id"))}},
                                  clipRefProps()),
                       {QStringLiteral("effect")}) },
        { "remove_effect", "effects", "Remove a video effect",
          "Remove a video effect by stack index.",
          objectSchema(mergeProps({{QStringLiteral("index"), integerProp(QStringLiteral("Effect index"))}},
                                  clipRefProps()),
                       {QStringLiteral("index")}) },
        { "set_effect_param", "effects", "Tweak a video effect",
          "Set one numeric/boolean video effect parameter.",
          objectSchema(mergeProps({{QStringLiteral("index"), integerProp(QStringLiteral("Effect index"))},
                                   {QStringLiteral("key"), stringProp(QStringLiteral("Parameter key"))},
                                   {QStringLiteral("value"), numberProp(QStringLiteral("Value (booleans as 0/1)"))}},
                                  clipRefProps()),
                       {QStringLiteral("index"), QStringLiteral("key"), QStringLiteral("value")}) },
        { "add_audio_effect", "effects", "Put an audio effect on a clip",
          "Append an audio effect to a clip.",
          objectSchema(mergeProps({{QStringLiteral("effect"), stringProp(QStringLiteral("Audio effect catalog id"))}},
                                  clipRefProps()),
                       {QStringLiteral("effect")}) },
        { "remove_audio_effect", "effects", "Remove an audio effect",
          "Remove an audio effect by stack index.",
          objectSchema(mergeProps({{QStringLiteral("index"), integerProp(QStringLiteral("Effect index"))}},
                                  clipRefProps()),
                       {QStringLiteral("index")}) },
        { "set_audio_effect_param", "effects", "Tweak an audio effect",
          "Set one audio effect parameter.",
          objectSchema(mergeProps({{QStringLiteral("index"), integerProp(QStringLiteral("Effect index"))},
                                   {QStringLiteral("key"), stringProp(QStringLiteral("Parameter key"))},
                                   {QStringLiteral("value"), numberProp(QStringLiteral("Value"))}},
                                  clipRefProps()),
                       {QStringLiteral("index"), QStringLiteral("key"), QStringLiteral("value")}) },
        { "add_transition", "effects", "Bridge two adjacent clips",
          "Add or replace a transition after the given clip (needs a neighbour).",
          objectSchema(mergeProps({{QStringLiteral("kind"), stringProp(QStringLiteral("Transition id (default crossfade)"))},
                                   {QStringLiteral("duration"), numberProp(QStringLiteral("Seconds (ignored when clips already overlap)"))}},
                                  clipRefProps())) },
        { "remove_transition", "effects", "Remove a transition",
          "Remove a transition by id from a track.",
          objectSchema({{QStringLiteral("track"), integerProp(QStringLiteral("Track index"))},
                        {QStringLiteral("id"), stringProp(QStringLiteral("Transition id"))}},
                       {QStringLiteral("track"), QStringLiteral("id")}) },
    };
    return k;
}

QJsonObject opTool(const Op &op)
{
    return toolDef(QString::fromUtf8(op.name), QString::fromUtf8(op.description), op.schema);
}

} // namespace

QStringList toolboxNames()
{
    return {QStringLiteral("media"),    QStringLiteral("timeline"), QStringLiteral("canvas"),
            QStringLiteral("playback"), QStringLiteral("text"),     QStringLiteral("effects")};
}

QJsonObject catalogPayload()
{
    struct Box {
        const char *name;
        const char *when;
    };
    static const Box boxes[] = {
        {"media", "Import and inspect the media bin before placing clips."},
        {"timeline", "Tracks, place/move/trim/split/delete clips, overlap toggle, undo."},
        {"canvas", "On-screen position, size, rotation, opacity."},
        {"playback", "Seek, play, pause, In/Out work area. Use capture (homepage) to see the frame."},
        {"text", "Add and edit title/caption clips."},
        {"effects", "Video/audio effects and transitions."},
    };

    QJsonArray toolboxes;
    for (const Box &box : boxes) {
        QJsonArray opNames;
        for (const Op &op : ops()) {
            if (qstrcmp(op.toolbox, box.name) == 0)
                opNames.append(QString::fromUtf8(op.name));
        }
        toolboxes.append(QJsonObject{
            {QStringLiteral("name"), QString::fromUtf8(box.name)},
            {QStringLiteral("when"), QString::fromUtf8(box.when)},
            {QStringLiteral("ops"), opNames},
        });
    }

    return ok({
        {QStringLiteral("toolboxes"), toolboxes},
        {QStringLiteral("hint"),
         QStringLiteral("toolbox({name}) then apply({ops:[{tool,args}…]}) for a batch. "
                        "inspect({clips:true}) for clip ids. capture() for a still.")},
    });
}

QJsonObject toolboxPayload(const QString &name)
{
    const QString key = name.trimmed().toLower();
    if (!toolboxNames().contains(key))
        return err("unknown_toolbox", QStringLiteral("Known: %1").arg(toolboxNames().join(QLatin1Char(' '))));

    QJsonArray tools;
    for (const Op &op : ops()) {
        if (key == QLatin1String(op.toolbox))
            tools.append(opTool(op));
    }
    return ok({{QStringLiteral("name"), key}, {QStringLiteral("tools"), tools}});
}

QJsonArray homepageTools()
{
    QJsonArray tools;
    tools.append(toolDef(QStringLiteral("catalog"),
                         QStringLiteral("Homepage: toolboxes, when to use each, and op names (no schemas). Call this first."),
                         objectSchema({})));
    tools.append(toolDef(QStringLiteral("toolbox"),
                         QStringLiteral("Load one toolbox: returns full JSON schemas for its ops. Then call those ops via apply."),
                         objectSchema({{QStringLiteral("name"),
                                        stringProp(QStringLiteral("media|timeline|canvas|playback|text|effects"))}},
                                      {QStringLiteral("name")})));
    tools.append(toolDef(QStringLiteral("inspect"),
                         QStringLiteral("Compact project state (size, playhead, overlap, work area). Pass clips=true for per-clip rows (id, track, start, duration)."),
                         objectSchema({{QStringLiteral("clips"),
                                        boolProp(QStringLiteral("Include per-clip rows"))}})));
    tools.append(toolDef(
        QStringLiteral("apply"),
        QStringLiteral("Run one or many ops in order. Stops on first error. One undo for the successful mutations. "
                       "ops: [{tool, args}]. Tool names come from catalog/toolbox."),
        objectSchema({{QStringLiteral("ops"),
                       arrayProp(objectSchema({{QStringLiteral("tool"), stringProp(QStringLiteral("Op name"))},
                                               {QStringLiteral("args"),
                                                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}}}},
                                              {QStringLiteral("tool")}),
                                 QStringLiteral("Sequential operations"))}},
                     {QStringLiteral("ops")})));
    tools.append(toolDef(QStringLiteral("capture"),
                         QStringLiteral("Still of the composition. Default: JPEG long-edge 1280 for analysis. full=true writes a PNG and returns its path."),
                         objectSchema({{QStringLiteral("at"), numberProp(QStringLiteral("Timeline seconds (default: playhead)"))},
                                       {QStringLiteral("full"), boolProp(QStringLiteral("Full-res PNG on disk instead of inline JPEG"))}})));
    return tools;
}

QJsonArray toolboxDirectTools(const QString &name)
{
    const QString key = name.trimmed().toLower();
    QJsonArray tools;
    for (const Op &op : ops()) {
        if (key == QLatin1String(op.toolbox))
            tools.append(opTool(op));
    }
    return tools;
}

bool isHomepageTool(const QString &name)
{
    static const QStringList k = {QStringLiteral("catalog"), QStringLiteral("toolbox"),
                                  QStringLiteral("inspect"), QStringLiteral("apply"),
                                  QStringLiteral("capture")};
    return k.contains(name);
}

bool isKnownOp(const QString &name)
{
    for (const Op &op : ops()) {
        if (name == QLatin1String(op.name))
            return true;
    }
    return false;
}

QString toolboxForOp(const QString &name)
{
    for (const Op &op : ops()) {
        if (name == QLatin1String(op.name))
            return QString::fromUtf8(op.toolbox);
    }
    return {};
}

QString homepageHtml()
{
    QString body = QStringLiteral(
        "<!doctype html><meta charset=utf-8><title>Drift MCP</title>"
        "<body style='font:14px/1.45 system-ui;max-width:42rem;margin:2rem auto;padding:0 1rem'>"
        "<h1>Drift agent access</h1>"
        "<p>This editor is exposing an MCP server on localhost. Any local process with the "
        "session token can edit the open project and capture frames.</p>"
        "<p>Agents: POST JSON-RPC to <code>/mcp</code> with "
        "<code>Authorization: Bearer …</code>. Call <code>catalog</code>, then "
        "<code>toolbox</code>, then <code>apply</code>.</p>"
        "<h2>Toolboxes</h2><ul>");
    const QJsonArray boxes = catalogPayload().value(QStringLiteral("toolboxes")).toArray();
    for (const QJsonValue &v : boxes) {
        const QJsonObject b = v.toObject();
        body += QStringLiteral("<li><strong>%1</strong> — %2<br><code>%3</code></li>")
                    .arg(b.value(QStringLiteral("name")).toString(),
                         b.value(QStringLiteral("when")).toString(),
                         [&] {
                             QStringList names;
                             for (const QJsonValue &n : b.value(QStringLiteral("ops")).toArray())
                                 names.append(n.toString());
                             return names.join(QStringLiteral(", "));
                         }());
    }
    body += QStringLiteral("</ul><p>Pinned endpoints: <code>/mcp/media</code>, "
                           "<code>/mcp/timeline</code>, <code>/mcp/canvas</code>, "
                           "<code>/mcp/playback</code>, <code>/mcp/text</code>, "
                           "<code>/mcp/effects</code>.</p></body>");
    return body;
}

} // namespace drift::mcp
