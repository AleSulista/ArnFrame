# Agent access (Drift MCP)

Drift can expose a localhost MCP server so Cursor, Claude Code, or other agents can edit the open project. Enable it in **Settings → Agent access** (off at every launch).

## Connect

**Cursor / Claude (this session):** copy the snippet from Settings after enabling. The bearer token rotates each session.

**One-time stdio attach** (no token in `mcp.json`):

```json
{
  "mcpServers": {
    "drift": {
      "command": "/path/to/drift",
      "args": ["--mcp-stdio"]
    }
  }
}
```

`drift --mcp-stdio` attaches to a running editor with Agent access enabled.

## Workflow

1. **`catalog`** — toolboxes, per-op “when” hints, endpoints, units, limitations (no schemas).
2. **`toolbox({name})`** — full JSON schemas for ops in that toolbox.
3. **`apply({ops:[{tool, args}, …]})`** — run mutations in order; one undo step for the batch.
4. **`inspect({clips:true, detail:true})`** — project state, clip UUIDs, effects, transitions, bookmarks, async jobs.
5. **`capture()`** — JPEG still of the composition (use to verify edits).

Homepage endpoint: `POST /mcp` with `Authorization: Bearer <token>`.

Pinned endpoints (`/mcp/timeline`, `/mcp/project`, …) list that toolbox’s ops directly. `catalog`, `toolbox`, and `apply` are only on `/mcp`; `inspect` and `capture` work on both. Toolbox ops can also be called by name directly on `/mcp` instead of through `apply` — but only `apply` collapses a batch into one undo step.

`apply` takes **toolbox ops only**. `catalog`, `toolbox`, `inspect`, `capture`, and `apply` itself return `unknown_op` inside an `ops` array; call them directly.

## Conventions

| Topic | Rule |
|-------|------|
| Time | Seconds |
| Clip reference | Prefer `clip` UUID from `inspect`; else `track` (0 = top) + `index`. One or the other is **required** — clip ops never fall back to the selection |
| Selection ops | `separate_audio`, `unlink_audio`, `merge_clips`, `align_clip_left/right`, `copy_selection`, `cut_selection`, `paste_at_playhead`, `freeze_frame` take no clip argument — call `select_clip` first |
| Discovery | Effect stack indices, transition ids, and bookmark indices exist **only** in `inspect({clips:true, detail:true})` |
| Overlap | Off by default — place/move snap to gaps unless `set_overlap` enables overlap; the reply reports `requested` vs `placed` |
| Export | `export_video` is async — poll `inspect().export` or `export_status`. The output path is normalised, so use the `path` echoed back |
| Atomicity | `apply` is **not** atomic: on failure the ops before it stay applied. Check `stopped` / `failed` / `done` |
| Undo | One batch = one undo step, except `import_media`, `save_project`, `set_overlap`, `set_theme`, `set_shortcut`, `reset_shortcuts` and the `audio` read ops |
| Errors | `{ok:false, error:<code>, detail:<text>}` — `bad_args`, `not_found`, `type_mismatch`, `unknown_op`, `unknown_toolbox`, `wrong_endpoint`, `wrong_toolbox`, `apply_failed`, `import_failed`, `import_timeout`, `export_busy`, `export_failed`, `export_timeout`, `capture_failed`, `conflict` |
| Change detection | Every `inspect` includes `revision`; pass `since:<revision>` to get `{unchanged:true}` when state is current |
| Media import | Local absolute paths, `file://` URLs, or `import_media_bytes` (base64 — always pass an explicit `path`) |

### Async jobs

All return `{started:true}` immediately. Every field below except `export` needs `inspect({detail:true})`.

| Started by | Poll |
|---|---|
| `export_video` | `export.{active, progress}` (or `export_status`) |
| `package_project` | `package.{active, progress}` |
| `generate_subtitles` | `subtitleGen.{active, progress, status}` |
| `set_clip_reverse` (video) | `reverseRender.{active, progress, status}` |
| `run_segmentation`, `segment_clip`, `apply_denoise`, `detect_faces` | No progress field — re-read `inspect({clips:true, detail:true})` and compare |

## Toolbox reference

| Toolbox | When to use |
|---------|-------------|
| `media` | Import (paths/bytes), list, rename, remove, replace, export still |
| `timeline` | Tracks, clips, selection, bookmarks, copy/paste, A/V link |
| `canvas` | Transform, flip, blend, mask, fade, speed, reverse, animation |
| `playback` | Seek, play, pause, In/Out work area |
| `text` | Title and caption clips |
| `shapes` | Builtin shapes, stickers, emoji, fonts, text presets |
| `subtitles` | Subtitle clips, cues, import/export, Whisper generation |
| `effects` | Video/audio effects, transitions, templates |
| `project` | Open/new/save/package, canvas, background, metadata, export |
| `keyframes` | Property animation keys and tangents |
| `speed` | Speed ramps; reading custom fade curves (write them with `set_fade_curve` in `canvas`) |
| `segmentation` | SAM-style cutout (session or one-shot) |
| `ai` | Denoise and face detection |
| `audio` | Waveforms, beat detection, beat-synced cuts, clip volume |
| `ui` | Theme, shortcuts, editor preferences |

### Working to the music

`detect_beats` **blocks** and returns the grid — no polling.

```json
{"name": "detect_beats", "arguments": {"start": 0, "duration": 30}}
```

```json
{"ok": true, "bpm": 128.0, "confidence": 0.81, "beatsPerBar": 4, "firstDownbeat": 2,
 "beats": [0.31, 0.78, 1.25], "onsets": [{"at": 0.31, "s": 0.9}], "cached": false}
```

Then either drive edits from those exact times, or arm the grid and let snapping do it:

| Call | Effect |
|---|---|
| `set_beat_layers({grid:true})` | Beats become snap targets — `place_clip`, `move_clip` and `move_to_track` now magnet to the nearest beat within 150 ms, and so do the user's own drags |
| `split_on_beats({clip, unit:"bar"})` | Cuts the clip at every bar line. One undo step |
| `snap_clips_to_beats({clips, unit:"beat"})` | Quantises clip starts. One undo step |
| `bookmark_beats({unit:"bar"})` | Writes the grid into the project as markers that outlive the analysis |

The analysis is **transient**: any edit that changes the mix drops it (`finishEdit` clears it when the audio fingerprint moves). `inspect({detail:true}).beats` reports `{active, analysed, bpm, confidence, rangeStart, rangeDuration, n, onsets, gridVisible, onsetsVisible, stale}` — check `stale` before trusting a grid you fetched a few ops ago, and re-run `detect_beats` when it is `true`.

## Traps

- **`set_transform` writes at the playhead.** If the property is keyframed, or `autoKey` is on, it creates a keyframe there instead of a constant value. Seek first, or mute the animation with `set_property_keyframes_enabled(false)`.
- **`set_mask` replaces the whole mask.** Omitted keys revert to defaults and omitting `shape` turns the mask off. Read the current mask and send it back merged.
- **`set_subtitle_cues` replaces every cue.** Same pattern — read, merge, send.
- **`set_effect_param`, `set_audio_effect_param`, `set_transition_param` do not validate.** A wrong key or index still returns `ok`. Verify with `inspect({clips:true, detail:true})`.
- **`set_speed_curve` returns a new clip id.** The old UUID stops resolving.
- **`remove_keyframe` deletes the *nearest* key** with no distance limit. Confirm the exact time with `list_keyframes`.
- **`set_keyframe_interpolation` moves the playhead** to `at`, changing the default time of later ops in the same batch.
- **`add_track` shifts every track index** — index 0 is the new track.
- **`import_media_bytes` without `path`** writes to a temp dir that is deleted when the call returns, leaving a broken asset. Always pass `path`.
- **`get_waveform` reads two different things.** With `clip` or `asset` it reads the *source file*, so clip speed, reverse and volume are not applied. Only the timeline form (`start` + `duration`, no clip or asset) is what you would actually hear.
- **`bpm: 0` from `detect_beats` is not an error** — it means no trustworthy tempo, so `beats` is empty. The `onsets` are still valid; pass `unit:"onset"` anywhere a grid is accepted.
- **Beat analysis dies on the next edit.** Read the whole grid out of `detect_beats` before mutating anything, or re-detect. `split_on_beats` and `snap_clips_to_beats` already snapshot it internally.
- **`snap_clips_to_beats` may not land on the beat.** With overlap off, a clip is pushed to the next free gap. Read the `to` values back rather than assuming they equal the beat time.
- **`split_on_beats` keeps your clip id for the *first* piece.** The other pieces are new UUIDs, returned in `clips` in timeline order.
- **There is no track volume.** `set_volume` is per clip; mute a whole lane with `set_track({muted:true})`.

## Example

An `ops` array is submitted whole, so it cannot reference an id produced earlier in the same batch. Import and place first, then read the new clip's id out of the reply:

```json
{
  "ops": [
    {"tool": "import_media", "args": {"paths": ["/path/to/clip.mp4"]}},
    {"tool": "place_clip", "args": {"asset": "0", "at": 0}}
  ]
}
```

The reply carries each op's result in order — `done[1].result.id` is the new clip's UUID. Use it in the next call:

```json
{
  "ops": [
    {"tool": "set_duration", "args": {"clip": "<done[1].result.id>", "duration": 5}},
    {"tool": "set_transform", "args": {"clip": "<same id>", "x": 0, "y": 0, "w": 1920, "h": 1080}}
  ]
}
```

Then verify visually — `capture` is a homepage tool and returns `unknown_op` inside `ops`, so call it on its own:

```json
{"name": "capture", "arguments": {"at": 2.5}}
```

## Security

- Binds to `127.0.0.1` only.
- Any local process with the session token has full editor access.
- Turn Agent access off when finished.

**Flatpak:** host file import may need `flatpak override --filesystem=home org.cutwire.Drift`.
