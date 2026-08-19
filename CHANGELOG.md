# Unreleased changes

Tracks work done on `main` **since the last public release**. Use this to see what is already fixed or added before filing an issue. Cleared when a new release ships.

**Last released version:** `0.2.2`

---

## ✅ Fixed

### Bug [`4670837`](https://github.com/CutWire-Studios/Drift/commit/4670837) Black video preview on macOS

The video preview stayed black. `NSOpenGLContext` will not share objects between the legacy 2.1 context Qt defaults to and the core 3.3 one the compositor uses, so the composited texture was never valid in the scene graph.

### Bug [`4d9ee4a`](https://github.com/CutWire-Studios/Drift/commit/4d9ee4a) Mouse wheel scrolls the panel instead of the slider

Scrolling over a property slider also scrolled the panel behind it, so the value jumped and the page moved. Sliders now consume the wheel (closes [#37](https://github.com/CutWire-Studios/Drift/issues/37)).

### Bug [`a34364f`](https://github.com/CutWire-Studios/Drift/commit/a34364f) Preview quality setting ignored

Half / Quarter / Auto preview quality did not change what the compositor actually rendered. The preview now matches the selected quality.

### Bug [`4b815db`](https://github.com/CutWire-Studios/Drift/commit/4b815db) Rotation tool in the preview

Dragging a rotated clip moved it on the clip’s own axes instead of the canvas, so the box slid off the pointer. The rotate grip now tracks the cursor, shows the live angle, and snaps to 15° steps (hold Ctrl to rotate freely).

### Bug [`3940d13`](https://github.com/CutWire-Studios/Drift/commit/3940d13) New project left the previous editor state behind

Starting a new project did not fully reset playback, selection, and panel state from the last project.

### Bug [`ac40967`](https://github.com/CutWire-Studios/Drift/commit/ac40967) / [`621e9d0`](https://github.com/CutWire-Studios/Drift/commit/621e9d0) Vertical video size and assets list view

Phone videos with a rotation display matrix were placed and thumbnailed as if they were already landscape, so a 1080×1920 clip in a 1920×1080 project became a square. The assets panel’s list-view button also did nothing (closes [#35](https://github.com/CutWire-Studios/Drift/issues/35)).

### Bug [`905c1a5`](https://github.com/CutWire-Studios/Drift/commit/905c1a5) Transition reset when clicking it

Clicking a transition on the timeline (or clicking away after dropping one) reverted it to the default crossfade (closes [#53](https://github.com/CutWire-Studios/Drift/issues/53)).

### Bug [`612234a`](https://github.com/CutWire-Studios/Drift/commit/612234a) No audio on Windows

Playback was silent when the output device would not accept the project’s sample rate or format. Drift now negotiates a format the device can play, and Settings has an **Audio output** picker (closes [#54](https://github.com/CutWire-Studios/Drift/issues/54)).

### Bug [`0fe7283`](https://github.com/CutWire-Studios/Drift/commit/0fe7283) Audio drifted out of sync where two clips from the same file overlap

Overlapping two clips cut from one media file — splitting a clip and dragging the halves together, the usual way to hand-build a crossfade — glitched the audio through the overlap and then left the second clip’s audio offset from its picture for the rest of its length. Both clips read through a single decode cursor, so the second one was handed the first one’s audio. Each clip now decodes on its own cursor. The same fix repositions audio after a forward seek shorter than two seconds, which previously kept playing from the old spot.

### Bug [`c844584`](https://github.com/CutWire-Studios/Drift/commit/c844584) Overlapping clips from the same file played back slowly

Where two clips cut from one file overlapped, every composited frame dragged the shared decoder between two positions seconds apart, and it decoded its way through most of a keyframe interval each time. On a 720p source that was 5.3 seconds of decoding for 100 frames, against 0.22 seconds for the same overlap between two separate files. Overlaps now cost the same either way.

---

## ✨ Added

### Feature Multicam editing

Cut a multi-camera shoot by watching every camera at once. Import your cameras, open the multicam window (`Ctrl+Shift+C`, or the grid button in the header) and click **Set up from my media** — each camera goes on its own track with an empty program track above them. Then click a tile, or press its number key, to cut the program over to that camera at the playhead.

It is a second view of the timeline you already have, not a separate editor: the playhead, playback and undo are shared with the main window in both directions, and a switch is one ordinary, undoable edit on the program track. Cuts change the picture only by default, so a separate audio bed plays straight through them; "Switch audio too" changes that.

### Feature [`ba70db1`](https://github.com/CutWire-Studios/Drift/commit/ba70db1) In/Out work area and ranged export

Mark an In and Out on the timeline to limit playback and export to that range. The export dialog can encode the work area only.

### Feature [`09c7f73`](https://github.com/CutWire-Studios/Drift/commit/09c7f73) Animated GIF export

The export dialog has a GIF mode (no audio). Mark a work area for a short loop, or export up to 60 seconds.

### Feature [`ea5929b`](https://github.com/CutWire-Studios/Drift/commit/ea5929b) macOS support

The build produces a `Drift.app` bundle, `scripts/package-macos.sh` writes a signed and notarised `.dmg`, and CI publishes an Apple Silicon disk image alongside the other platforms. See the [macOS section of the README](README.md#macos) for prerequisites and known limits.

### Feature [`7daea92`](https://github.com/CutWire-Studios/Drift/commit/7daea92) Portable Windows build

Releases include a `Drift-Portable-*.zip` that can be extracted anywhere and run without the installer.

### Feature [`f430eee`](https://github.com/CutWire-Studios/Drift/commit/f430eee) Portrait workspace layout

A vertical panel layout for 9:16 projects: preview on top, media and inspector below, timeline at the bottom. Switch from the header; portrait canvases default to it (closes [#46](https://github.com/CutWire-Studios/Drift/issues/46)).

### Feature [`3a0e1be`](https://github.com/CutWire-Studios/Drift/commit/3a0e1be) Built-in MCP server for local agents

An opt-in localhost MCP server lets Cursor, Claude Code, and similar agents inspect and edit the open project (timeline, effects, keyframes, speed, export, audio / beat tools, and more). Enable it in **Settings → Agent access**. See [MCP.md](MCP.md) (closes [#47](https://github.com/CutWire-Studios/Drift/issues/47)).

### Feature [`efac334`](https://github.com/CutWire-Studios/Drift/commit/efac334) / [`d56b166`](https://github.com/CutWire-Studios/Drift/commit/d56b166) Multilingual UI

Menus and labels go through Qt Linguist. Pick a language in Settings, or leave **System default** to follow the OS locale. Sinhala ships alongside English; further languages can be added as `i18n/drift_<lang>.ts` catalogs (closes [#51](https://github.com/CutWire-Studios/Drift/issues/51)).

### Feature [`b592e88`](https://github.com/CutWire-Studios/Drift/commit/b592e88) 1440p quality tier

The project layout chooser now offers 1440p (2560×1440 / 1440×2560) between 1080p and 4K.

### Feature [`1981e6b`](https://github.com/CutWire-Studios/Drift/commit/1981e6b) Preview zoom and pan

Ctrl+scroll zooms the preview around the cursor; middle-drag pans. Click the zoom readout to reset to 100%.

### Feature [`69575a7`](https://github.com/CutWire-Studios/Drift/commit/69575a7) Move and delete keyframes

Keyframe diamonds on the graph can be dragged (time and value, with snap) and double-clicked to delete. The inspector row has previous / next / add / remove controls at the playhead.

### Feature [`64a83ce`](https://github.com/CutWire-Studios/Drift/commit/64a83ce) Caption length when generating subtitles

Auto-captions can be packed to a chosen word count per cue (including 1–3 word short-form captions) instead of only the default line width (closes [#45](https://github.com/CutWire-Studios/Drift/issues/45)).

---

## 🎨 Improved

### [`168a44f`](https://github.com/CutWire-Studios/Drift/commit/168a44f) Quieter logs by default

FFmpeg and Qt debug chatter is hidden unless you pass `--verbose` or set `DRIFT_VERBOSE=1`. Warnings still print.

### [`280d09f`](https://github.com/CutWire-Studios/Drift/commit/280d09f) Kinetic scrolling on category rails

Horizontal category lists (effects, stickers, emoji, and similar) keep a short momentum scroll after a flick.
