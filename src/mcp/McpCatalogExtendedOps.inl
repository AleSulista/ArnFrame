        ,
        { "remove_asset", "media", "Delete a bin row",
          "Remove an asset from the media bin. Clips already placed on the timeline keep playing and "
          "are NOT deleted.",
          objectSchema({{QStringLiteral("asset"), assetRefProp()}},
                       {QStringLiteral("asset")}),
          false, true },
        { "replace_asset", "media", "Swap source file",
          "Point an existing bin row at a different file. Every clip using the asset switches to the "
          "new media, keeping its trim and timeline position — trims past the new file's duration are "
          "clamped downstream.",
          objectSchema({{QStringLiteral("asset"), assetRefProp()},
                        {QStringLiteral("path"), stringProp(QStringLiteral("Absolute path or file:// URL; must exist"))}},
                       {QStringLiteral("asset"), QStringLiteral("path")}) },
        { "export_asset_image", "media", "Export a still from an asset",
          "Write an image file from a bin asset (its poster frame for video). Creates parent folders. "
          "The format follows the path's extension.",
          objectSchema({{QStringLiteral("asset"), assetRefProp()},
                        {QStringLiteral("path"), stringProp(QStringLiteral("Absolute output path, extension picks the format"))}},
                       {QStringLiteral("asset"), QStringLiteral("path")}) },
        { "import_media_bytes", "media", "Import from base64",
          "Decode base64 bytes to a file, then import it like import_media. ALWAYS pass an explicit "
          "`path`: with only `name` the file is written to a temporary directory that is deleted as "
          "soon as the call returns, leaving the asset pointing at a missing file. Returns the same "
          "shape as import_media.",
          objectSchema({{QStringLiteral("data"), stringProp(QStringLiteral("Base64-encoded file bytes"))},
                        {QStringLiteral("name"), stringProp(QStringLiteral("Filename hint (e.g. clip.mp4). Used only when path is omitted."))},
                        {QStringLiteral("path"), stringProp(QStringLiteral("Absolute write path — strongly recommended; parent folders are created"))}},
                       {QStringLiteral("data")}) },

        { "load_project", "project", "Open a .dcut project",
          "Load a project file, replacing the open timeline. DISCARDS unsaved changes without warning "
          "and clears the undo stack — save_project first if the current project matters (inspect.dirty "
          "tells you).",
          objectSchema({{QStringLiteral("path"), stringProp(QStringLiteral("Absolute .dcut path"))}},
                       {QStringLiteral("path")}),
          false, true },
        { "new_project", "project", "Start empty",
          "Discard the current timeline and create an empty project. DISCARDS unsaved changes without "
          "warning and clears the undo stack — check inspect.dirty and save_project first.",
          objectSchema({}), false, true },
        { "package_project", "project", "Save bundled copy",
          "Write a copy of the project with all media embedded. Async: returns {started:true, path} "
          "immediately — poll inspect({detail:true}).package.{active,progress} until active is false.",
          objectSchema({{QStringLiteral("path"), stringProp(QStringLiteral("Absolute output .dcut path"))}},
                       {QStringLiteral("path")}) },
        { "cancel_package", "project", "Stop packaging",
          "Cancel an in-flight package job. Returns ok even when nothing was running; confirm with "
          "inspect({detail:true}).package.active.",
          objectSchema({}) },
        { "cancel_export", "project", "Stop encoding",
          "Cancel an in-flight export. Returns ok even when nothing was running; confirm with "
          "export_status. Use this before a new export_video if one is already busy.",
          objectSchema({}) },
        { "apply_canvas_crop", "project", "Crop canvas",
          "Crop the project canvas to a pixel rectangle, changing the project width/height. Clip "
          "transforms are rebased onto the new canvas. width and height must be > 0.",
          objectSchema({{QStringLiteral("x"), numberProp(QStringLiteral("Left pixels"))},
                        {QStringLiteral("y"), numberProp(QStringLiteral("Top pixels"))},
                        {QStringLiteral("width"), numberProp(QStringLiteral("Width pixels, must be > 0"))},
                        {QStringLiteral("height"), numberProp(QStringLiteral("Height pixels, must be > 0"))}},
                       {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"), QStringLiteral("height")}) },

        { "move_track", "timeline", "Reorder lanes",
          "Move a track from one index to another; the tracks between them shift. Every track index "
          "and every track+index clip reference you hold is invalidated — re-read inspect after this.",
          objectSchema({{QStringLiteral("from"), integerProp(QStringLiteral("Source track index"))},
                        {QStringLiteral("to"), integerProp(QStringLiteral("Destination track index"))}},
                       {QStringLiteral("from"), QStringLiteral("to")}) },
        { "set_track_waveform", "timeline", "Show/hide waveform",
          "Toggle waveform display on an audio/video track row. Cosmetic — does not affect rendering "
          "or export.",
          objectSchema({{QStringLiteral("track"), integerProp(QStringLiteral("Track index"))},
                        {QStringLiteral("show"), boolProp(QStringLiteral("Show waveform"))}},
                       {QStringLiteral("track"), QStringLiteral("show")}) },
        { "set_track_height", "timeline", "Resize a lane",
          "Set the track's row height multiplier, clamped to 0.6–4.0. Cosmetic — does not affect "
          "rendering or export.",
          objectSchema({{QStringLiteral("track"), integerProp(QStringLiteral("Track index"))},
                        {QStringLiteral("scale"), numberProp(QStringLiteral("Height multiplier, clamped 0.6..4.0"))}},
                       {QStringLiteral("track"), QStringLiteral("scale")}) },
        { "select_clip", "timeline", "Focus one clip",
          "Select exactly one clip, replacing any previous selection. REQUIRED before the "
          "selection-based ops, which take no clip argument: separate_audio, unlink_audio, "
          "merge_clips, align_clip_left, align_clip_right, copy_selection, cut_selection.",
          objectSchema(clipRefProps()) },
        { "clear_selection", "timeline", "Deselect all",
          "Clear the clip selection. Selection-based ops fail afterwards until you select again.",
          objectSchema({}) },
        { "select_all_clips", "timeline", "Select everything",
          "Select every clip on the timeline. Useful before copy_selection or cut_selection.",
          objectSchema({}) },
        { "copy_selection", "timeline", "Copy to clipboard",
          "Copy the selected clips to the internal clipboard. Acts on the current selection — call "
          "select_clip or select_all_clips first. Paste with paste_at_playhead.",
          objectSchema({}) },
        { "cut_selection", "timeline", "Cut to clipboard",
          "Cut the selected clips to the internal clipboard, removing them from the timeline. Acts on "
          "the current selection — call select_clip or select_all_clips first.",
          objectSchema({}), false, true },
        { "paste_at_playhead", "timeline", "Paste clips",
          "Paste the internal clipboard at the playhead. Requires an earlier copy_selection or "
          "cut_selection; returns ok with no effect when the clipboard is empty. Does not return the "
          "new clip ids — diff inspect({clips:true}) to find them.",
          objectSchema({}) },
        { "separate_audio", "timeline", "Split A/V",
          "Detach the audio of the selected video clip onto its own audio track. Acts on the current "
          "selection — call select_clip first; fails bad_args when nothing separable is selected.",
          objectSchema({}) },
        { "unlink_audio", "timeline", "Break A/V link",
          "Break the link between paired audio/video clips in the selection so they can be moved and "
          "trimmed independently. Acts on the current selection — call select_clip first; fails "
          "bad_args when no linked clips are selected.",
          objectSchema({}) },
        { "merge_clips", "timeline", "Join adjacent clips",
          "Merge the selected adjacent clips on one track back into a single clip. Acts on the current "
          "selection — select the clips first; fails bad_args when the selection cannot be merged.",
          objectSchema({}) },
        { "align_clip_left", "timeline", "Snap start to zero",
          "Move the selected clip so it starts at 0. Acts on the current selection — call select_clip "
          "first.",
          objectSchema({}) },
        { "align_clip_right", "timeline", "Snap end to duration",
          "Move the selected clip so it ends at the project duration. Acts on the current selection — "
          "call select_clip first.",
          objectSchema({}) },
        { "set_clip_name", "timeline", "Rename a clip",
          "Set the clip's display name on the timeline. Cosmetic; does not touch the bin asset.",
          objectSchema(mergeProps({{QStringLiteral("name"), stringProp(QStringLiteral("New name, must be non-empty"))}},
                                  clipRefProps()),
                       {QStringLiteral("name")}) },
        { "add_bookmark", "timeline", "Mark a time",
          "Add a bookmark at `at` seconds. Does not return its index — bookmarks are addressed by "
          "position in inspect({detail:true}).bookmarks, so re-read that before removing or updating.",
          objectSchema({{QStringLiteral("at"), numberProp(QStringLiteral("Seconds"))},
                        {QStringLiteral("label"), stringProp(QStringLiteral("Label"))}},
                       {QStringLiteral("at")}) },
        { "remove_bookmark", "timeline", "Delete bookmark",
          "Remove a bookmark by its position in inspect({detail:true}).bookmarks. Later bookmarks shift "
          "down one index.",
          objectSchema({{QStringLiteral("index"), bookmarkIndexProp()}},
                       {QStringLiteral("index")}),
          false, true },
        { "update_bookmark", "timeline", "Edit bookmark",
          "Update a bookmark's time and/or label. Pass BOTH at and label: an omitted field is read back "
          "from the existing bookmark, and an out-of-range index is not checked on that path. Verify "
          "the index against inspect({detail:true}).bookmarks first.",
          objectSchema({{QStringLiteral("index"), bookmarkIndexProp()},
                        {QStringLiteral("at"), numberProp(QStringLiteral("Seconds; omitted keeps the current time"))},
                        {QStringLiteral("label"), stringProp(QStringLiteral("Label; omitted keeps the current label"))}},
                       {QStringLiteral("index")}) },
        { "go_to_bookmark", "timeline", "Seek to bookmark",
          "Move the playhead to a bookmark, addressed by its position in "
          "inspect({detail:true}).bookmarks.",
          objectSchema({{QStringLiteral("index"), bookmarkIndexProp()}},
                       {QStringLiteral("index")}) },
        { "freeze_frame", "timeline", "Hold current frame",
          "Insert a still of the frame under the playhead as a new clip. Uses the playhead, not a clip "
          "argument — seek first. Does not return the new clip id; diff inspect({clips:true}).",
          objectSchema({}) },

        { "set_flip", "canvas", "Mirror a clip",
          "Set horizontal and/or vertical flip. Omitted axes keep their current value. At least one of "
          "flipH/flipV is required.",
          objectSchema(mergeProps({{QStringLiteral("flipH"), boolProp(QStringLiteral("Flip horizontally"))},
                                   {QStringLiteral("flipV"), boolProp(QStringLiteral("Flip vertically"))}},
                                  clipRefProps())) },
        { "set_blend_mode", "canvas", "Change compositing",
          "Set how the clip composites over the layers below it. Unrecognised values silently fall back "
          "to normal.",
          objectSchema(mergeProps({{QStringLiteral("mode"),
                                    enumProp(QStringLiteral("Blend mode; unknown values fall back to normal"),
                                             {QStringLiteral("normal"), QStringLiteral("multiply"),
                                              QStringLiteral("screen"), QStringLiteral("overlay"),
                                              QStringLiteral("add"), QStringLiteral("darken"),
                                              QStringLiteral("lighten")})}},
                                  clipRefProps()),
                       {QStringLiteral("mode")}) },
        { "set_mask", "canvas", "Clip mask",
          "Set the clip's mask. This REPLACES the whole mask — it is not a patch. Every omitted key "
          "reverts to its default, and omitting `shape` turns the mask off entirely, so read the "
          "current mask from inspect({clips:true,detail:true}) and send it back with your changes "
          "merged in.",
          objectSchema(mergeProps({{QStringLiteral("mask"), maskSchema()}},
                                  clipRefProps()),
                       {QStringLiteral("mask")}) },
        { "set_fade", "canvas", "Audio/video fade",
          "Set fade-in and fade-out lengths in seconds. Omitted ends keep their current value. Use "
          "set_fade_curve to change the fade's shape.",
          objectSchema(mergeProps({{QStringLiteral("in"), numberProp(QStringLiteral("Fade in seconds"))},
                                   {QStringLiteral("out"), numberProp(QStringLiteral("Fade out seconds"))}},
                                  clipRefProps())) },
        { "set_fade_curve", "canvas", "Fade shape",
          "Set the shape of this clip's fades, either by preset (`curve`) or by explicit control "
          "points (`points`, which selects the custom curve). Exactly one of the two is required; when "
          "both are sent, points wins. Passing points opens and closes a transient curve session and "
          "closes any fade-curve session already open. A clip whose fade animation is set to Fade "
          "inherits this curve.",
          objectSchema(mergeProps(
              {{QStringLiteral("curve"),
                enumProp(QStringLiteral("Fade curve preset. Unknown values fall back to smooth."),
                         {QStringLiteral("linear"), QStringLiteral("smooth"),
                          QStringLiteral("equalPower"), QStringLiteral("custom")})},
               {QStringLiteral("points"),
                arrayProp(objectSchema({{QStringLiteral("t"), numberProp(QStringLiteral("Time 0..1 across the fade"))},
                                        {QStringLiteral("g"), numberProp(QStringLiteral("Gain 0..1"))}}),
                          QStringLiteral("Custom curve points, at least two. Selects the custom curve."))}},
              clipRefProps())) },
        { "set_clip_speed", "canvas", "Constant speed",
          "Set a single playback rate for the whole clip (1.0 = normal, 2.0 = double speed). Retimes "
          "the clip's timeline duration. Clears any speed curve — use set_speed_curve for a ramp.",
          objectSchema(mergeProps({{QStringLiteral("speed"), numberProp(QStringLiteral("Playback rate, e.g. 0.5 half speed, 2.0 double"))}},
                                  clipRefProps()),
                       {QStringLiteral("speed")}) },
        { "set_clip_reverse", "canvas", "Reverse playback",
          "Play the clip backwards. For video this kicks off an async proxy render — poll "
          "inspect({detail:true}).reverseRender.{active,progress,status} until active is false before "
          "exporting, and cancel it with cancel_reverse_render.",
          objectSchema(mergeProps({{QStringLiteral("reverse"), boolProp(QStringLiteral("Reversed"))}},
                                  clipRefProps()),
                       {QStringLiteral("reverse")}) },
        { "cancel_reverse_render", "canvas", "Stop reverse proxy",
          "Cancel the in-flight reverse proxy render. Returns ok even when nothing was running; "
          "confirm with inspect({detail:true}).reverseRender.active.",
          objectSchema({}) },
        { "set_animation", "canvas", "In/out motion",
          "Set the clip's entrance (animIn) or exit (animOut) animation. Only the supplied fields "
          "change. Setting kind to fade makes the animation follow the clip's fade curve.",
          objectSchema(mergeProps({{QStringLiteral("which"), enumProp(QStringLiteral("Which end of the clip to animate"),
                                                                       {QStringLiteral("animIn"),
                                                                        QStringLiteral("animOut")})},
                                   {QStringLiteral("kind"),
                                    enumProp(QStringLiteral("Animation kind; unknown values fall back to none"),
                                             {QStringLiteral("none"), QStringLiteral("fade"),
                                              QStringLiteral("slideUp"), QStringLiteral("slideDown"),
                                              QStringLiteral("slideLeft"), QStringLiteral("slideRight"),
                                              QStringLiteral("zoomIn"), QStringLiteral("zoomOut"),
                                              QStringLiteral("pop"), QStringLiteral("spinCW"),
                                              QStringLiteral("spinCCW"), QStringLiteral("bounce")})},
                                   {QStringLiteral("duration"), numberProp(QStringLiteral("Seconds"))},
                                   {QStringLiteral("ease"),
                                    enumProp(QStringLiteral("Easing; unknown values fall back to easeOut"),
                                             {QStringLiteral("linear"), QStringLiteral("easeInOut"),
                                              QStringLiteral("easeOut"), QStringLiteral("back")})},
                                   {QStringLiteral("curve"),
                                    enumProp(QStringLiteral("Fade curve used when kind is fade"),
                                             {QStringLiteral("linear"), QStringLiteral("smooth"),
                                              QStringLiteral("equalPower"), QStringLiteral("custom")})}},
                                  clipRefProps()),
                       {QStringLiteral("which")}) },
        { "set_shape_style", "canvas", "Shape look",
          "Patch the fill, stroke, and geometry of a shape clip. Only supplied keys change. Applies to "
          "shape clips only — silently does nothing on any other clip type. Numeric fields are clamped "
          "to the ranges in the schema, and each geometry knob is read by a subset of shape kinds only.",
          objectSchema(mergeProps({{QStringLiteral("style"), shapeStyleSchema()}},
                                  clipRefProps()),
                       {QStringLiteral("style")}) },

        { "list_shapes", "shapes", "Builtin shapes",
          "Returns {shapes:[{id, label, cat}]}. Use id with add_shape or as shape_style.kind.",
          objectSchema({}), true, false, true },
        { "list_stickers", "shapes", "Builtin stickers",
          "Returns {stickers:[{id, label, cat}]}. Use id with add_sticker.",
          objectSchema({}), true, false, true },
        { "list_emoji", "shapes", "Emoji catalog",
          "Returns {emoji:[{id, label}]}. add_emoji takes the emoji character itself, not the id.",
          objectSchema({}), true, false, true },
        { "list_text_presets", "shapes", "Text style packs",
          "Returns {presets:[{id, label}]}. Use id as the preset argument to add_text.",
          objectSchema({}), true, false, true },
        { "list_fonts", "shapes", "Font list",
          "Returns {fonts:[{id, label}]} for fonts available on this machine. Use the family name as "
          "style.fontFamily in set_text.",
          objectSchema({}), true, false, true },
        { "add_shape", "shapes", "Place a shape",
          "Add a builtin shape clip, creating a shape track when needed. Does NOT return the new clip "
          "id — diff inspect({clips:true}) to address it afterwards, then style it with "
          "set_shape_style.",
          objectSchema({{QStringLiteral("shape"), stringProp(QStringLiteral("Shape id from list_shapes"))},
                        {QStringLiteral("at"), numberProp(QStringLiteral("Start seconds (default: playhead)"))},
                        {QStringLiteral("track"), integerProp(QStringLiteral("Optional destination track; omitted picks or creates one"))}},
                       {QStringLiteral("shape")}) },
        { "add_sticker", "shapes", "Place a sticker",
          "Add a sticker clip, creating a track when needed. Does NOT return the new clip id — diff "
          "inspect({clips:true}) to address it afterwards.",
          objectSchema({{QStringLiteral("sticker"), stringProp(QStringLiteral("Sticker id from list_stickers"))},
                        {QStringLiteral("at"), numberProp(QStringLiteral("Start seconds (default: playhead)"))}},
                       {QStringLiteral("sticker")}) },
        { "add_emoji", "shapes", "Place emoji",
          "Add an emoji clip, creating a track when needed. Takes the emoji CHARACTER (e.g. \"🎬\"), not "
          "a catalog id. Does NOT return the new clip id — diff inspect({clips:true}).",
          objectSchema({{QStringLiteral("emoji"), stringProp(QStringLiteral("The emoji character itself, e.g. 🎬"))},
                        {QStringLiteral("name"), stringProp(QStringLiteral("Optional display name for the clip"))},
                        {QStringLiteral("at"), numberProp(QStringLiteral("Start seconds (default: playhead)"))}},
                       {QStringLiteral("emoji")}) },

        { "add_subtitle_clip", "subtitles", "Empty subtitle lane",
          "Add an empty subtitle clip, creating a subtitle track when needed. Does NOT return the new "
          "clip id — diff inspect({clips:true}), then fill it with set_subtitle_cues or "
          "import_subtitle_into_clip.",
          objectSchema({{QStringLiteral("at"), numberProp(QStringLiteral("Start seconds (default: playhead)"))}}) },
        { "import_subtitle_file", "subtitles", "Load SRT/VTT",
          "Import an .srt or .vtt file as a NEW subtitle clip. Does not return the clip id — diff "
          "inspect({clips:true}). Use import_subtitle_into_clip to fill an existing clip instead.",
          objectSchema({{QStringLiteral("path"), stringProp(QStringLiteral("Absolute .srt or .vtt path"))},
                        {QStringLiteral("at"), numberProp(QStringLiteral("Start seconds (default: playhead)"))}},
                       {QStringLiteral("path")}) },
        { "import_subtitle_into_clip", "subtitles", "Load into clip",
          "Import a subtitle file into an existing subtitle clip, replacing its cues.",
          objectSchema(mergeProps({{QStringLiteral("path"), stringProp(QStringLiteral("Absolute .srt or .vtt path"))}},
                                  clipRefProps()),
                       {QStringLiteral("path")}) },
        { "export_subtitle_file", "subtitles", "Write subtitles",
          "Write a subtitle clip's cues to a file. The format follows the path's extension (.srt/.vtt).",
          objectSchema(mergeProps({{QStringLiteral("path"), stringProp(QStringLiteral("Absolute output path; extension picks the format"))}},
                                  clipRefProps()),
                       {QStringLiteral("path")}) },
        { "set_subtitle_cues", "subtitles", "Replace all cues",
          "REPLACE every cue on a subtitle clip. This is not a merge — cues you leave out are deleted, "
          "so read the current list from inspect({clips:true,detail:true}) first when editing. Times "
          "are timeline seconds.",
          objectSchema(mergeProps(
              {{QStringLiteral("cues"),
                arrayProp(objectSchema({{QStringLiteral("start"), numberProp(QStringLiteral("Start seconds"))},
                                        {QStringLiteral("end"), numberProp(QStringLiteral("End seconds"))},
                                        {QStringLiteral("text"), stringProp(QStringLiteral("Caption"))}}),
                           QStringLiteral("Full cue list; replaces the existing one"))}},
              clipRefProps()),
                       {QStringLiteral("cues")}) },
        { "upsert_subtitle_cue", "subtitles", "Edit cue at playhead",
          "Set the text of the cue under the playhead, creating one if there is none. Uses the "
          "playhead, not a time argument — seek first.",
          objectSchema(mergeProps({{QStringLiteral("text"), stringProp(QStringLiteral("Caption text, must be non-empty"))}},
                                  clipRefProps()),
                       {QStringLiteral("text")}) },
        { "list_whisper_languages", "subtitles", "Whisper languages",
          "Returns {languages:[{id, label}]}. Pass an id as generate_subtitles.language.",
          objectSchema({}), true, false, true },
        { "generate_subtitles", "subtitles", "Auto transcribe",
          "Transcribe a clip's audio with Whisper into a new subtitle clip. Async: returns "
          "{started:true} immediately — poll "
          "inspect({detail:true}).subtitleGen.{active,progress,status} until active is false. Cancel "
          "with cancel_subtitle_generation.",
          objectSchema(mergeProps({{QStringLiteral("language"), stringProp(QStringLiteral("Language id from list_whisper_languages; omitted auto-detects"))}},
                                  clipRefProps())) },
        { "cancel_subtitle_generation", "subtitles", "Stop Whisper",
          "Cancel in-flight subtitle generation. Returns ok even when nothing was running; confirm "
          "with inspect({detail:true}).subtitleGen.active.",
          objectSchema({}) },

        { "set_effect_enabled", "effects", "Toggle effect",
          "Bypass or re-enable a video effect without removing it. The effect keeps its stack position "
          "and parameters.",
          objectSchema(mergeProps({{QStringLiteral("index"), effectIndexProp()},
                                   {QStringLiteral("enabled"), boolProp(QStringLiteral("Enabled"))}},
                                  clipRefProps()),
                       {QStringLiteral("index"), QStringLiteral("enabled")}) },
        { "move_effect", "effects", "Reorder effect",
          "Move a video effect to a different stack position. Effects render in stack order, so this "
          "changes the result. All indices between from and to shift.",
          objectSchema(mergeProps({{QStringLiteral("from"), effectIndexProp()},
                                   {QStringLiteral("to"), integerProp(QStringLiteral("Destination stack position"))}},
                                  clipRefProps()),
                       {QStringLiteral("from"), QStringLiteral("to")}) },
        { "set_effect_color_param", "effects", "Color effect param",
          "Set a color-typed effect parameter. Use this instead of set_effect_param for params whose "
          "type is color in list_effects. Not validated — an unknown key or bad index still returns ok.",
          objectSchema(mergeProps({{QStringLiteral("index"), effectIndexProp()},
                                   {QStringLiteral("key"), stringProp(QStringLiteral("Parameter key from list_effects"))},
                                   {QStringLiteral("value"), stringProp(QStringLiteral("Color #RRGGBB or #AARRGGBB"))}},
                                  clipRefProps()),
                       {QStringLiteral("index"), QStringLiteral("key"), QStringLiteral("value")}) },
        { "set_audio_effect_enabled", "effects", "Toggle audio effect",
          "Bypass or re-enable an audio effect without removing it.",
          objectSchema(mergeProps({{QStringLiteral("index"), effectIndexProp()},
                                   {QStringLiteral("enabled"), boolProp(QStringLiteral("Enabled"))}},
                                  clipRefProps()),
                       {QStringLiteral("index"), QStringLiteral("enabled")}) },
        { "move_audio_effect", "effects", "Reorder audio effect",
          "Move an audio effect to a different stack position. Audio effects process in stack order.",
          objectSchema(mergeProps({{QStringLiteral("from"), effectIndexProp()},
                                   {QStringLiteral("to"), integerProp(QStringLiteral("Destination stack position"))}},
                                  clipRefProps()),
                       {QStringLiteral("from"), QStringLiteral("to")}) },
        { "list_effect_templates", "effects", "Effect packs",
          "Returns {templates:[{id, label, cat}]} — preset bundles of effects and parameters. Use id "
          "with apply_effect_template.",
          objectSchema({}), true, false, true },
        { "apply_effect_template", "effects", "Apply template",
          "Apply a preset effect pack to a clip, appending its effects to the clip's stack.",
          objectSchema(mergeProps({{QStringLiteral("template"), stringProp(QStringLiteral("Template id from list_effect_templates"))}},
                                  clipRefProps()),
                       {QStringLiteral("template")}) },
        { "set_transition_kind", "effects", "Change transition type",
          "Swap an existing transition to another kind. This clears its parameter overrides.",
          objectSchema({{QStringLiteral("track"), integerProp(QStringLiteral("Track index the transition lives on"))},
                        {QStringLiteral("id"), transitionIdProp()},
                        {QStringLiteral("kind"), stringProp(QStringLiteral("Transition id from list_transitions"))}},
                       {QStringLiteral("track"), QStringLiteral("id"), QStringLiteral("kind")}) },
        { "set_transition_duration", "effects", "Transition length",
          "Set a transition's length in seconds. Ignored when the two clips physically overlap — the "
          "overlap dictates the duration there.",
          objectSchema({{QStringLiteral("track"), integerProp(QStringLiteral("Track index"))},
                        {QStringLiteral("id"), transitionIdProp()},
                        {QStringLiteral("duration"), numberProp(QStringLiteral("Seconds"))}},
                       {QStringLiteral("track"), QStringLiteral("id"), QStringLiteral("duration")}) },
        { "set_transition_param", "effects", "Transition param",
          "Set one numeric transition parameter. Not validated — an unknown key or id still returns "
          "ok. Take keys from the transition's params in list_transitions and verify with "
          "inspect({clips:true,detail:true}).",
          objectSchema({{QStringLiteral("track"), integerProp(QStringLiteral("Track index"))},
                        {QStringLiteral("id"), transitionIdProp()},
                        {QStringLiteral("key"), stringProp(QStringLiteral("Param key from list_transitions"))},
                        {QStringLiteral("value"), numberProp(QStringLiteral("Value"))}},
                       {QStringLiteral("track"), QStringLiteral("id"), QStringLiteral("key"),
                        QStringLiteral("value")}) },

        { "move_keyframe", "keyframes", "Move a key",
          "Move a keyframe from one time to another, optionally changing its value. `from` must match "
          "an existing key — confirm with list_keyframes. When value is omitted the property's current "
          "value at `from` is carried over.",
          objectSchema(mergeProps({{QStringLiteral("prop"), animPropProp()},
                                   {QStringLiteral("from"), numberProp(QStringLiteral("Existing key's timeline seconds"))},
                                   {QStringLiteral("to"), numberProp(QStringLiteral("New timeline seconds"))},
                                   {QStringLiteral("value"), numberProp(QStringLiteral("Optional new value; omitted keeps the value at `from`"))}},
                                  clipRefProps()),
                       {QStringLiteral("prop"), QStringLiteral("from"), QStringLiteral("to")}) },

        { "list_fade_curve", "speed", "Read custom fade shape",
          "Returns {points:[{t,g}]} for the clip's custom fade curve. Despite being a read, this opens "
          "and closes a transient session and will CLOSE any fade-curve session already open. Fails "
          "bad_args on clips that cannot carry a custom fade. Write with set_fade_curve (canvas).",
          objectSchema(clipRefProps()), false, false, true },

        { "segmentation_status", "segmentation", "Check availability",
          "Returns {available, model}. Check available is true before any other segmentation op — the "
          "model ships separately and the session ops do not report its absence.",
          objectSchema({}), true, false, true },
        { "begin_segmentation_session", "segmentation", "Open segment UI state",
          "Open an interactive segmentation session on one frame of a clip. REQUIRED before "
          "set_segmentation_frame, add/remove/clear_segmentation_point, and run_segmentation — those "
          "ops do not check, and return ok while doing nothing when no session is open. Close with "
          "end_segmentation_session. Prefer segment_clip for a single one-shot cutout.",
          objectSchema(mergeProps({{QStringLiteral("at"), numberProp(QStringLiteral("Frame seconds (default: playhead)"))},
                                   {QStringLiteral("forTemplate"), boolProp(QStringLiteral("Template mode: produce a reusable mask instead of editing this clip"))}},
                                  clipRefProps())) },
        { "end_segmentation_session", "segmentation", "Close session",
          "End the segmentation session, discarding its points without applying them. Always call this "
          "when abandoning a session — a stale session interferes with later segmentation ops.",
          objectSchema({}) },
        { "set_segmentation_frame", "segmentation", "Pick frame",
          "Move the segmentation preview to another frame. Requires an open session from "
          "begin_segmentation_session; returns ok with no effect otherwise.",
          objectSchema({{QStringLiteral("at"), numberProp(QStringLiteral("Seconds"))}},
                       {QStringLiteral("at")}) },
        { "add_segmentation_point", "segmentation", "Add SAM point",
          "Add an include or exclude hint point. Requires an open session; returns ok with no effect "
          "otherwise. Coordinates are normalised 0..1 across the frame, not pixels.",
          objectSchema({{QStringLiteral("x"), numberProp(QStringLiteral("X, 0..1 across the frame"))},
                        {QStringLiteral("y"), numberProp(QStringLiteral("Y, 0..1 down the frame"))},
                        {QStringLiteral("include"), propWithDefault(boolProp(QStringLiteral("true keeps this region, false excludes it")), true)}},
                       {QStringLiteral("x"), QStringLiteral("y")}) },
        { "remove_segmentation_point", "segmentation", "Remove SAM point",
          "Remove a hint point by the order it was added. Requires an open session; returns ok with no "
          "effect otherwise.",
          objectSchema({{QStringLiteral("index"), integerProp(QStringLiteral("0-based position in the order points were added"))}},
                       {QStringLiteral("index")}) },
        { "clear_segmentation_points", "segmentation", "Clear points",
          "Remove all hint points but keep the session open. Requires an open session.",
          objectSchema({}) },
        { "run_segmentation", "segmentation", "Run from session",
          "Run segmentation using the session's current points. Requires an open session. Async and "
          "with no progress field — re-read inspect({clips:true,detail:true}) and compare to detect "
          "completion. Cancel with cancel_segmentation.",
          objectSchema({{QStringLiteral("output"),
                         propWithDefault(enumProp(QStringLiteral("clips splits the subject onto its own clip; mask writes a mask onto the existing clip"),
                                                  {QStringLiteral("clips"), QStringLiteral("mask")}),
                                         QStringLiteral("clips"))}}) },
        { "segment_clip", "segmentation", "One-shot segment",
          "Segment a clip with explicit points in a single call — no session needed. Prefer this over "
          "the session ops for scripted use. Async and with no progress field: re-read "
          "inspect({clips:true,detail:true}) and compare to detect completion.",
          objectSchema(mergeProps(
              {{QStringLiteral("points"),
                arrayProp(objectSchema({{QStringLiteral("x"), numberProp(QStringLiteral("X, 0..1 across the frame"))},
                                        {QStringLiteral("y"), numberProp(QStringLiteral("Y, 0..1 down the frame"))},
                                        {QStringLiteral("include"), boolProp(QStringLiteral("true keeps this region, false excludes it (default true)"))}}),
                           QStringLiteral("Hint points, at least one"))},
               {QStringLiteral("output"),
                propWithDefault(enumProp(QStringLiteral("clips splits the subject onto its own clip; mask writes a mask onto the existing clip"),
                                         {QStringLiteral("clips"), QStringLiteral("mask")}),
                                QStringLiteral("clips"))}},
              clipRefProps()),
                       {QStringLiteral("points")}) },
        { "cancel_segmentation", "segmentation", "Stop segment job",
          "Cancel an in-flight segmentation. Returns ok even when nothing was running.",
          objectSchema({}) },

        { "denoise_status", "ai", "Denoise availability",
          "Returns {available}. Check this before apply_denoise — the model ships separately.",
          objectSchema({}), true, false, true },
        { "apply_denoise", "ai", "Denoise clip",
          "Run AI audio denoise over the clip. Async and with no progress field — re-read "
          "inspect({clips:true,detail:true}) and compare to detect completion. Cancel with "
          "cancel_denoise.",
          objectSchema(clipRefProps()) },
        { "cancel_denoise", "ai", "Cancel denoise",
          "Cancel an in-flight denoise. Returns ok even when nothing was running.",
          objectSchema({}) },
        { "face_detection_status", "ai", "Face track availability",
          "Returns {available}. Check this before detect_faces — the model ships separately.",
          objectSchema({}), true, false, true },
        { "detect_faces", "ai", "Detect faces",
          "Run face detection over a clip and store the result as a face track for auto-framing. Async "
          "and with no progress field — re-read inspect({clips:true,detail:true}) and compare to detect "
          "completion. Cancel with cancel_face_detection.",
          objectSchema(clipRefProps()) },
        { "cancel_face_detection", "ai", "Cancel face detect",
          "Cancel an in-flight face detection. Returns ok even when nothing was running.",
          objectSchema({}) },
        { "clear_face_track", "ai", "Remove face track",
          "Delete the stored face track from a clip, undoing detect_faces.",
          objectSchema(clipRefProps()), false, true },

        { "set_ui_preferences", "ui", "Editor flags",
          "Set editor preferences; only supplied keys change. Note followSystem only acts when TRUE "
          "(it clears the dark-mode override); passing false does nothing — use set_theme to pin a "
          "theme. autoKey matters for set_transform: when on, transform writes become keyframes at the "
          "playhead. Returns the resulting flags plus theme {overridden, dark}.",
          objectSchema({{QStringLiteral("autoKey"), boolProp(QStringLiteral("Auto-keyframe: transform edits create keyframes at the playhead"))},
                        {QStringLiteral("mediaGrid"), boolProp(QStringLiteral("Media bin grid mode"))},
                        {QStringLiteral("reopenLastProject"), boolProp(QStringLiteral("Reopen last project on launch"))},
                        {QStringLiteral("followSystem"), boolProp(QStringLiteral("true clears the theme override so the OS theme wins; false is a no-op"))}}) }
