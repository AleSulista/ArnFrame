#include "mcp/McpCatalog.h"
#include "mcp/McpJson.h"

namespace drift::mcp {
namespace {

const QStringList kTrackTypes = {QStringLiteral("video"), QStringLiteral("audio"),
                                 QStringLiteral("text"), QStringLiteral("subtitle"),
                                 QStringLiteral("shape")};

QJsonObject clipRefProps()
{
    return {
        {QStringLiteral("clip"),
         stringProp(QStringLiteral("Clip UUID (preferred, stable). Resolve clip by clip id first."))},
        {QStringLiteral("track"),
         integerProp(QStringLiteral("Track index if clip id omitted (0 = top)"))},
        {QStringLiteral("index"),
         integerProp(QStringLiteral("Clip index on that track if clip id omitted"))},
    };
}

QJsonObject mergeProps(QJsonObject a, const QJsonObject &b)
{
    for (auto it = b.begin(); it != b.end(); ++it)
        a.insert(it.key(), it.value());
    return a;
}

QJsonObject textStyleSchema()
{
    return objectSchema({
        {QStringLiteral("fontFamily"), stringProp(QStringLiteral("Font family name"))},
        {QStringLiteral("fontWeight"), integerProp(QStringLiteral("Font weight (e.g. 400, 700)"))},
        {QStringLiteral("pixelSize"), numberProp(QStringLiteral("Font size in pixels"))},
        {QStringLiteral("color"), stringProp(QStringLiteral("Text color (#RRGGBB or #AARRGGBB)"))},
        {QStringLiteral("italic"), boolProp(QStringLiteral("Italic"))},
        {QStringLiteral("align"), enumProp(QStringLiteral("Horizontal alignment"),
                                          {QStringLiteral("left"), QStringLiteral("center"),
                                           QStringLiteral("right")})},
        {QStringLiteral("valign"), enumProp(QStringLiteral("Vertical alignment"),
                                            {QStringLiteral("top"), QStringLiteral("center"),
                                             QStringLiteral("bottom")})},
        {QStringLiteral("lineHeight"), numberProp(QStringLiteral("Line height multiplier"))},
        {QStringLiteral("letterSpacing"), numberProp(QStringLiteral("Letter spacing"))},
        {QStringLiteral("wordWrap"), boolProp(QStringLiteral("Wrap long lines"))},
    });
}

QJsonObject speedPointSchema()
{
    return objectSchema({
        {QStringLiteral("pos"), numberProp(QStringLiteral("Normalised position 0..1 over trimmed source"))},
        {QStringLiteral("speed"), numberProp(QStringLiteral("Playback rate at this point"))},
        {QStringLiteral("inDx"), numberProp(QStringLiteral("Incoming tangent dx (curve space)"))},
        {QStringLiteral("inDy"), numberProp(QStringLiteral("Incoming tangent dy"))},
        {QStringLiteral("outDx"), numberProp(QStringLiteral("Outgoing tangent dx"))},
        {QStringLiteral("outDy"), numberProp(QStringLiteral("Outgoing tangent dy"))},
        {QStringLiteral("corner"), boolProp(QStringLiteral("Break tangent collinearity"))},
    });
}

QJsonObject exportSettingsProps()
{
    return {
        {QStringLiteral("scale"), stringProp(QStringLiteral("Scale id from list_export_options"))},
        {QStringLiteral("height"), integerProp(QStringLiteral("Target height in pixels; 0 = project height"))},
        {QStringLiteral("fps"), numberProp(QStringLiteral("Output fps; 0 = project rate"))},
        {QStringLiteral("video"), stringProp(QStringLiteral("Video codec id (h264, hevc, …)"))},
        {QStringLiteral("audio"), stringProp(QStringLiteral("Audio codec id (aac, opus, …)"))},
        {QStringLiteral("crf"), integerProp(QStringLiteral("Quality when using CRF (lower is better)"))},
        {QStringLiteral("bitrate"), integerProp(QStringLiteral("Video kbps when using bitrate mode"))},
        {QStringLiteral("audio_bitrate"), integerProp(QStringLiteral("Audio kbps"))},
        {QStringLiteral("audio_only"), boolProp(QStringLiteral("Encode audio only"))},
        {QStringLiteral("gif"), boolProp(QStringLiteral("Encode animated GIF"))},
        {QStringLiteral("work_area"), boolProp(QStringLiteral("Limit to In/Out work area"))},
        {QStringLiteral("in"), numberProp(QStringLiteral("Range start seconds"))},
        {QStringLiteral("out"), numberProp(QStringLiteral("Range end seconds"))},
    };
}

struct Op {
    const char *name;
    const char *toolbox;
    const char *when;
    const char *description;
    QJsonObject schema;
    bool readOnly = false;
    bool destructive = false;
    bool idempotent = false;
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
          "List imported assets. Returns [{index, id, name, kind, dur, w, h}].",
          objectSchema({}), true, false, true },
        { "rename_asset", "media", "Rename a bin row",
          "Rename an asset in the bin. Does not rename the file on disk.",
          objectSchema({{QStringLiteral("asset"), stringProp(QStringLiteral("Asset id or bin index as string"))},
                        {QStringLiteral("name"), stringProp(QStringLiteral("New display name"))}},
                       {QStringLiteral("asset"), QStringLiteral("name")}) },

        { "add_track", "timeline", "Need a new lane",
          "Prepend a track. New track becomes index 0.",
          objectSchema({{QStringLiteral("type"), enumProp(QStringLiteral("Track type"), kTrackTypes)}},
                       {QStringLiteral("type")}) },
        { "remove_track", "timeline", "Delete a lane and its clips",
          "Delete a track and everything on it.",
          objectSchema({{QStringLiteral("track"), integerProp(QStringLiteral("Track index"))}},
                       {QStringLiteral("track")}),
          false, true },
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
          objectSchema(clipRefProps()), false, true },
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
          "Set text content and/or a partial style patch. Only supplied style keys change.",
          objectSchema(mergeProps({{QStringLiteral("text"), stringProp(QStringLiteral("New content"))},
                                   {QStringLiteral("style"), textStyleSchema()}},
                                  clipRefProps())) },

        { "list_effects", "effects", "See available video effects",
          "Returns [{id, label, cat, params:[{key,label,type,min,max}]}]. Use id with add_effect; param keys with set_effect_param.",
          objectSchema({}), true, false, true },
        { "list_audio_effects", "effects", "See available audio effects",
          "Returns [{id, label, cat, params:[…]}]. Use id with add_audio_effect.",
          objectSchema({}), true, false, true },
        { "list_transitions", "effects", "See available transitions",
          "Returns [{kind, label, cat, params:[…]}]. Use kind with add_transition (default crossfade).",
          objectSchema({}), true, false, true },
        { "add_effect", "effects", "Put a video effect on a clip",
          "Append a video effect to a clip.",
          objectSchema(mergeProps({{QStringLiteral("effect"), stringProp(QStringLiteral("Effect catalog id"))}},
                                  clipRefProps()),
                       {QStringLiteral("effect")}) },
        { "remove_effect", "effects", "Remove a video effect",
          "Remove a video effect by stack index.",
          objectSchema(mergeProps({{QStringLiteral("index"), integerProp(QStringLiteral("Effect index"))}},
                                  clipRefProps()),
                       {QStringLiteral("index")}),
          false, true },
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
                       {QStringLiteral("index")}),
          false, true },
        { "set_audio_effect_param", "effects", "Tweak an audio effect",
          "Set one audio effect parameter (booleans as 0/1).",
          objectSchema(mergeProps({{QStringLiteral("index"), integerProp(QStringLiteral("Effect index"))},
                                   {QStringLiteral("key"), stringProp(QStringLiteral("Parameter key"))},
                                   {QStringLiteral("value"), numberProp(QStringLiteral("Value"))}},
                                  clipRefProps()),
                       {QStringLiteral("index"), QStringLiteral("key"), QStringLiteral("value")}) },
        { "add_transition", "effects", "Bridge two adjacent clips",
          "Add or replace a transition after the given clip (needs a neighbour). Default kind: crossfade.",
          objectSchema(mergeProps(
              {{QStringLiteral("kind"),
                propWithDefault(stringProp(QStringLiteral("Transition id from list_transitions")),
                                QStringLiteral("crossfade"))},
               {QStringLiteral("duration"), numberProp(QStringLiteral("Seconds (ignored when clips already overlap)"))}},
              clipRefProps())) },
        { "remove_transition", "effects", "Remove a transition",
          "Remove a transition by id from a track.",
          objectSchema({{QStringLiteral("track"), integerProp(QStringLiteral("Track index"))},
                        {QStringLiteral("id"), stringProp(QStringLiteral("Transition id"))}},
                       {QStringLiteral("track"), QStringLiteral("id")}),
          false, true },

        { "set_project_setup", "project", "Change canvas size or frame rate",
          "Set project width, height, and fps.",
          objectSchema({{QStringLiteral("width"), integerProp(QStringLiteral("Canvas width pixels"))},
                        {QStringLiteral("height"), integerProp(QStringLiteral("Canvas height pixels"))},
                        {QStringLiteral("fps"), integerProp(QStringLiteral("Frames per second"))}},
                       {QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("fps")}) },
        { "set_background", "project", "Change canvas background",
          "Set background kind, color (#AARRGGBB), and/or blur strength.",
          objectSchema({{QStringLiteral("kind"), enumProp(QStringLiteral("Background kind"),
                                                          {QStringLiteral("color"), QStringLiteral("blur")})},
                        {QStringLiteral("color"), stringProp(QStringLiteral("Background color #AARRGGBB"))},
                        {QStringLiteral("blurStrength"), numberProp(QStringLiteral("Blur amount 0..200"))}}) },
        { "set_metadata", "project", "Set project title and author",
          "Set project metadata. Omitted fields are left unchanged.",
          objectSchema({{QStringLiteral("title"), stringProp(QStringLiteral("Project title"))},
                        {QStringLiteral("author"), stringProp(QStringLiteral("Author name"))},
                        {QStringLiteral("description"), stringProp(QStringLiteral("Project description"))}}) },
        { "save_project", "project", "Save the project file",
          "Save to an absolute path. Sync; creates parent folders if needed.",
          objectSchema({{QStringLiteral("path"), stringProp(QStringLiteral("Absolute .dcut path"))}},
                       {QStringLiteral("path")}) },
        { "list_export_options", "project", "See codecs, scales, and fps choices",
          "Returns scales, fps ids, video/audio codecs, gif availability, and last export folder.",
          objectSchema({}), true, false, true },
        { "export_video", "project", "Render the timeline to a file",
          "Start async encode. Poll inspect.export or export_status for progress. Omitted settings use last export or defaults.",
          objectSchema(mergeProps({{QStringLiteral("path"), stringProp(QStringLiteral("Absolute output path"))}},
                                  exportSettingsProps()),
                       {QStringLiteral("path")}) },
        { "export_status", "project", "Check an in-flight encode",
          "Returns {busy, progress, message}. Also available as inspect.export.",
          objectSchema({}), true, false, true },

        { "list_animated_properties", "keyframes", "See what animates on a clip",
          "Returns property names with keyframe animation (x, y, width, height, rotation, opacity, volume, fx.N.param).",
          objectSchema(clipRefProps()), true, false, true },
        { "list_keyframes", "keyframes", "Read keys for one property",
          "Returns [{seconds, value, inDx, inDy, outDx, outDy, corner, hold, easing, custom}]. Times are timeline seconds.",
          objectSchema(mergeProps({{QStringLiteral("prop"),
                                    stringProp(QStringLiteral("Property: x, y, width, height, rotation, opacity, volume, or fx.N.key"))}},
                                  clipRefProps()),
                       {QStringLiteral("prop")}),
          true, false, true },
        { "set_keyframe", "keyframes", "Add or update a key",
          "Set a keyframe at timeline seconds. Property names match list_keyframes.",
          objectSchema(mergeProps({{QStringLiteral("prop"), stringProp(QStringLiteral("Animated property"))},
                                   {QStringLiteral("at"), numberProp(QStringLiteral("Timeline seconds"))},
                                   {QStringLiteral("value"), numberProp(QStringLiteral("Property value"))}},
                                  clipRefProps()),
                       {QStringLiteral("prop"), QStringLiteral("at"), QStringLiteral("value")}) },
        { "remove_keyframe", "keyframes", "Delete a key",
          "Remove the nearest keyframe at timeline seconds.",
          objectSchema(mergeProps({{QStringLiteral("prop"), stringProp(QStringLiteral("Animated property"))},
                                   {QStringLiteral("at"), numberProp(QStringLiteral("Timeline seconds"))}},
                                  clipRefProps()),
                       {QStringLiteral("prop"), QStringLiteral("at")}),
          false, true },
        { "set_keyframe_interpolation", "keyframes", "Set linear/hold/ease on a key",
          "Set easing preset on the key nearest `at` seconds (linear, hold, ease).",
          objectSchema(mergeProps(
              {{QStringLiteral("prop"), stringProp(QStringLiteral("Animated property"))},
               {QStringLiteral("at"), numberProp(QStringLiteral("Timeline seconds"))},
               {QStringLiteral("mode"), enumProp(QStringLiteral("Interpolation"),
                                                {QStringLiteral("linear"), QStringLiteral("hold"),
                                                 QStringLiteral("ease")})}},
              clipRefProps()),
                       {QStringLiteral("prop"), QStringLiteral("at"), QStringLiteral("mode")}) },
        { "set_keyframe_tangents", "keyframes", "Shape bezier handles",
          "Set tangent handles in seconds / property units relative to the key at `at`.",
          objectSchema(mergeProps(
              {{QStringLiteral("prop"), stringProp(QStringLiteral("Animated property"))},
               {QStringLiteral("at"), numberProp(QStringLiteral("Timeline seconds"))},
               {QStringLiteral("inDx"), numberProp(QStringLiteral("Incoming handle dx seconds"))},
               {QStringLiteral("inDy"), numberProp(QStringLiteral("Incoming handle dy"))},
               {QStringLiteral("outDx"), numberProp(QStringLiteral("Outgoing handle dx seconds"))},
               {QStringLiteral("outDy"), numberProp(QStringLiteral("Outgoing handle dy"))},
               {QStringLiteral("corner"), boolProp(QStringLiteral("Break tangent collinearity"))}},
              clipRefProps()),
                       {QStringLiteral("prop"), QStringLiteral("at")}) },
        { "set_keyframe_hold", "keyframes", "Step-hold a key",
          "When true the property holds until the next key.",
          objectSchema(mergeProps({{QStringLiteral("prop"), stringProp(QStringLiteral("Animated property"))},
                                   {QStringLiteral("at"), numberProp(QStringLiteral("Timeline seconds"))},
                                   {QStringLiteral("hold"), boolProp(QStringLiteral("Hold until next key"))}},
                                  clipRefProps()),
                       {QStringLiteral("prop"), QStringLiteral("at"), QStringLiteral("hold")}) },
        { "set_property_keyframes_enabled", "keyframes", "Mute animation without deleting keys",
          "When false keys are kept but the property holds its first key's value.",
          objectSchema(mergeProps({{QStringLiteral("prop"), stringProp(QStringLiteral("Animated property"))},
                                   {QStringLiteral("enabled"), boolProp(QStringLiteral("Keyframes drive the property"))}},
                                  clipRefProps()),
                       {QStringLiteral("prop"), QStringLiteral("enabled")}) },

        { "list_speed_curve", "speed", "Read a clip's speed ramp",
          "Returns {hasCurve, points:[{pos,speed,…}], retimedDuration}. Opens a transient read of the curve.",
          objectSchema(clipRefProps()), true, false, true },
        { "set_speed_curve", "speed", "Apply a custom speed ramp",
          "Replace the clip with a retimed copy carrying the curve. Needs at least two points. Returns new clip id.",
          objectSchema(mergeProps(
              {{QStringLiteral("points"),
                arrayProp(speedPointSchema(), QStringLiteral("Speed curve control points"))}},
              clipRefProps()),
                       {QStringLiteral("points")}) },
        { "clear_speed_curve", "speed", "Remove a speed ramp",
          "Clear the curve and restore scalar-speed timeline duration.",
          objectSchema(clipRefProps()), false, true },

        { "get_ui_preferences", "ui", "Read editor UI settings",
          "Returns theme {overridden, dark} and editor flags (autoKey, mediaGrid, reopenLastProject).",
          objectSchema({}), true, false, true },
        { "set_theme", "ui", "Set dark or light theme",
          "Set explicit dark-mode preference (does not follow OS until cleared in the app).",
          objectSchema({{QStringLiteral("dark"), boolProp(QStringLiteral("true = dark, false = light"))}},
                       {QStringLiteral("dark")}) },
        { "list_shortcuts", "ui", "List action bindings",
          "Returns [{id, label, shortcut}] for editor actions.",
          objectSchema({}), true, false, true },
        { "set_shortcut", "ui", "Rebind a shortcut",
          "Bind keys to an action id from list_shortcuts. Empty keys clears. Returns conflict label on failure.",
          objectSchema({{QStringLiteral("action"), stringProp(QStringLiteral("Action id"))},
                        {QStringLiteral("keys"), stringProp(QStringLiteral("Qt key sequence, e.g. Ctrl+S"))}},
                       {QStringLiteral("action"), QStringLiteral("keys")}) },
        { "reset_shortcuts", "ui", "Restore default shortcuts",
          "Reset every action binding to defaults.",
          objectSchema({}) },
    };
    return k;
}

QJsonObject opTool(const Op &op)
{
    return toolDef(QString::fromUtf8(op.name),
                   QStringLiteral("When: %1. %2").arg(QString::fromUtf8(op.when),
                                                      QString::fromUtf8(op.description)),
                   op.schema,
                   toolAnnotations(op.readOnly, op.destructive, op.idempotent));
}

QJsonArray endpointList()
{
    QJsonArray endpoints;
    endpoints.append(QStringLiteral("/mcp"));
    for (const QString &name : toolboxNames()) {
        if (name != QStringLiteral("mcp"))
            endpoints.append(QStringLiteral("/mcp/") + name);
    }
    return endpoints;
}

} // namespace

QStringList toolboxNames()
{
    return {QStringLiteral("media"),     QStringLiteral("timeline"), QStringLiteral("canvas"),
            QStringLiteral("playback"),  QStringLiteral("text"),     QStringLiteral("effects"),
            QStringLiteral("project"),   QStringLiteral("keyframes"), QStringLiteral("speed"),
            QStringLiteral("ui")};
}

QString agentGuideText()
{
    return QStringLiteral(
        "Drift MCP agent guide\n"
        "\n"
        "Workflow:\n"
        "1. Call catalog on POST /mcp (homepage).\n"
        "2. Call toolbox({name}) for JSON schemas of ops in that toolbox.\n"
        "3. Call apply({ops:[{tool, args}, …]}) to run one or many mutations in one undo step.\n"
        "4. Call inspect({clips:true}) for clip UUIDs, path, background, and export progress.\n"
        "5. Call capture() for a composition still (JPEG by default).\n"
        "\n"
        "Pinned endpoints (/mcp/media, /mcp/timeline, …) list toolbox ops directly. "
        "catalog, toolbox, and apply are only on /mcp.\n"
        "\n"
        "Conventions:\n"
        "- Times are seconds.\n"
        "- Prefer clip UUID from inspect; track index 0 is the top lane.\n"
        "- Clip overlap is off by default (place/move snap to gaps).\n"
        "- export_video is async; poll inspect.export until active is false.\n"
        "\n"
        "Toolboxes: media, timeline, canvas, playback, text, effects, project, keyframes, speed, ui.\n");
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
        {"project", "Canvas size, background, metadata, save, and export."},
        {"keyframes", "Animate clip and effect properties over time."},
        {"speed", "Variable playback speed (retimed clips)."},
        {"ui", "Editor theme and keyboard shortcuts."},
    };

    QJsonArray toolboxes;
    for (const Box &box : boxes) {
        QJsonArray opEntries;
        for (const Op &op : ops()) {
            if (qstrcmp(op.toolbox, box.name) == 0) {
                opEntries.append(QJsonObject{
                    {QStringLiteral("name"), QString::fromUtf8(op.name)},
                    {QStringLiteral("when"), QString::fromUtf8(op.when)},
                });
            }
        }
        toolboxes.append(QJsonObject{
            {QStringLiteral("name"), QString::fromUtf8(box.name)},
            {QStringLiteral("when"), QString::fromUtf8(box.when)},
            {QStringLiteral("ops"), opEntries},
        });
    }

    return ok({
        {QStringLiteral("toolboxes"), toolboxes},
        {QStringLiteral("endpoints"), endpointList()},
        {QStringLiteral("units"),
         QJsonObject{{QStringLiteral("time"), QStringLiteral("seconds")},
                     {QStringLiteral("trackIndex"), QStringLiteral("0=top")},
                     {QStringLiteral("clipId"), QStringLiteral("stable UUID")}}},
        {QStringLiteral("workflow"),
         QStringLiteral("catalog → toolbox({name}) → apply({ops:[{tool,args}…]})")},
        {QStringLiteral("hint"),
         QStringLiteral("toolbox({name}) then apply({ops:[{tool,args}…]}) for a batch. "
                        "inspect({clips:true}) for clip ids. capture() for a still.")},
        {QStringLiteral("guide"), agentGuideText()},
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
    const QStringList toolboxEnum = toolboxNames();
    QJsonArray tools;
    tools.append(toolDef(QStringLiteral("catalog"),
                         QStringLiteral("When: Start here. Returns toolboxes, per-op when hints, endpoints, units, and workflow (no schemas)."),
                         objectSchema({}),
                         toolAnnotations(true, false, true)));
    tools.append(toolDef(
        QStringLiteral("toolbox"),
        QStringLiteral("When: Load schemas. Returns full JSON schemas for one toolbox's ops. Then call those ops via apply."),
        objectSchema({{QStringLiteral("name"), enumProp(QStringLiteral("Toolbox name"), toolboxEnum)}},
                     {QStringLiteral("name")})));
    tools.append(toolDef(
        QStringLiteral("inspect"),
        QStringLiteral("When: Read state. Returns revision, name, w, h, fps, dur, playhead, overlap, path, dirty, background, export {active, progress}, tracks, assets. Pass clips=true for per-clip rows (id, start, duration, trim). Pass since=<revision> to skip the payload when nothing changed."),
        objectSchema({{QStringLiteral("clips"), boolProp(QStringLiteral("Include per-clip rows"))},
                      {QStringLiteral("since"),
                       integerProp(QStringLiteral("Revision from a prior inspect; returns {unchanged:true} when current"))}}),
        toolAnnotations(true, false, true)));
    tools.append(toolDef(
        QStringLiteral("apply"),
        QStringLiteral("When: Mutate. Run one or many ops in order. Stops on first error. One undo for successful mutations. ops: [{tool, args}]."),
        objectSchema({{QStringLiteral("ops"),
                       arrayProp(objectSchema({{QStringLiteral("tool"), stringProp(QStringLiteral("Op name"))},
                                               {QStringLiteral("args"),
                                                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}}}},
                                              {QStringLiteral("tool")}),
                                 QStringLiteral("Sequential operations"))}},
                     {QStringLiteral("ops")})));
    tools.append(toolDef(
        QStringLiteral("capture"),
        QStringLiteral("When: Verify visually. Still of the composition. Default: JPEG long-edge 1280. full=true writes PNG to disk."),
        objectSchema({{QStringLiteral("at"), numberProp(QStringLiteral("Timeline seconds (default: playhead)"))},
                      {QStringLiteral("full"), boolProp(QStringLiteral("Full-res PNG on disk instead of inline JPEG"))}}),
        toolAnnotations(true, false, true)));
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
    const QJsonObject cat = catalogPayload();
    QString body = QStringLiteral(
        "<!doctype html><meta charset=utf-8><title>Drift MCP</title>"
        "<body style='font:14px/1.45 system-ui;max-width:42rem;margin:2rem auto;padding:0 1rem'>"
        "<h1>Drift agent access</h1>"
        "<p>This editor is exposing an MCP server on localhost. Any local process with the "
        "session token can edit the open project and capture frames.</p>"
        "<p><strong>Workflow:</strong> %1</p>"
        "<p>Agents: POST JSON-RPC to <code>/mcp</code> with "
        "<code>Authorization: Bearer …</code>.</p>"
        "<h2>Toolboxes</h2><ul>")
                    .arg(cat.value(QStringLiteral("workflow")).toString());
    const QJsonArray boxes = cat.value(QStringLiteral("toolboxes")).toArray();
    for (const QJsonValue &v : boxes) {
        const QJsonObject b = v.toObject();
        QStringList names;
        for (const QJsonValue &op : b.value(QStringLiteral("ops")).toArray())
            names.append(op.toObject().value(QStringLiteral("name")).toString());
        body += QStringLiteral("<li><strong>%1</strong> — %2<br><code>%3</code></li>")
                    .arg(b.value(QStringLiteral("name")).toString(),
                         b.value(QStringLiteral("when")).toString(),
                         names.join(QStringLiteral(", ")));
    }
    QStringList endpointStrings;
    for (const QJsonValue &ep : cat.value(QStringLiteral("endpoints")).toArray())
        endpointStrings.append(QStringLiteral("<code>%1</code>").arg(ep.toString()));
    body += QStringLiteral("</ul><p>Pinned endpoints: %1.</p></body>").arg(endpointStrings.join(QStringLiteral(", ")));
    return body;
}

} // namespace drift::mcp
