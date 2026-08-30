# Unreleased changes

Tracks work done on `main` **since the last public release**. Use this to see what is already fixed or added before filing an issue. Cleared when a new release ships.

**Last released version:** `0.4.0`

---

## ✅ Fixed

- Closing a project with unsaved changes now asks Save / Don't Save / Cancel instead of quitting silently and offering “Recovered unsaved work” on the next launch.
- Clicking a timestamp on the timeline during playback now moves the playhead there.
- Selected text clips can be dragged on the timeline again.
- Dragging media out of the bin onto the timeline works again.
- Leaving preview fullscreen restores a maximized window on KDE instead of shrinking it.
- Icons are SVG instead of PNG, so they stay sharp at high DPI and after interface scale.
- Images downscale with bilinear filtering instead of looking blocky.
- Hardware-decoded preview frames are drawn instead of being treated as empty (which forced a software fallback).
- Windows UI text (Inter) renders with FreeType so neighbouring letters and diacritics are no longer wrong.
- Dropping a clip onto the timeline can be undone with Ctrl+Z.
- Previously hardcoded UI strings can be translated.
- AV1 playback works on Android.

## ✨ Added

- Settings → Interface scale: 100–200% extra UI size (buttons, text, icons) on top of the system display scale. Takes effect after restart.
- Folders in the media bin: create, rename, nest, and navigate with a breadcrumb. Move clips with “Move to folder…”. Deleting a folder keeps its contents. Folder actions undo and redo with the rest of the project.
- Preview, trim, and crop a clip from the media bin (“Preview and edit…”) before putting it on the timeline. Save replaces that bin item.
- Face Swap effect: replace a face in a clip with a still photo, with opacity, edge feather, and match-lighting controls.
- Video stabilization: scan a clip and smooth camera shake, with a smoothness slider, optional tripod lock, and bake-or-keyframes modes.
- First launch asks which language to use. Language can also be changed any time from the header or Settings.
- Translations: Brazilian Portuguese, Chinese (Simplified), French (Canada), Spanish, Italian, and Sinhala.
- Settings → Faster preview (experimental): keep Intel/AMD VAAPI video on the GPU end to end. Off by default; takes effect after restart. Turn it off if the picture looks wrong.
- Android app: the phone UI now lives in this tree, including crop/trim in the bin and “decide layout later” when starting a project.
- Agents (MCP) can stabilize clips, close gaps, switch multicam, detect/remove silence, normalize loudness, duck music, auto-reframe, inspect and jump the undo stack, and save without passing a path.

## 🎨 Improved

- Preview stays on the GPU instead of copying every frame through the CPU. Full / Half / Quarter now match the preview panel size, so a 4K timeline in a small window is no longer composited at 4K.
- Export converts frames to the encoder format on the GPU and prepares the next frame while one is encoding. Typical 8-bit exports are faster; GIF, audio-only, and 10-bit still use the old path.
- Settings are grouped into Video, Preview, Playback, Interface, and App so related options sit together.
- Timeline navigation: scroll pans left and right, Shift+scroll moves between tracks, and middle-click drag also pans. Horizontal pan can be toggled in Settings → Interface.
