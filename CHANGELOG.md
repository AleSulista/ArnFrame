# Unreleased changes

Tracks work done on `main` **since the last public release**. Use this to see what is already fixed or added before filing an issue. Cleared when a new release ships.

**Last released version:** `0.1.2`

---

## ✅ Fixed

### Bug [`b17a209`](https://github.com/CutWire-Studios/Drift/commit/b17a209) File picker creates hidden files on GNOME Flatpak

In the Flatpak release, saving a file without a file extension created a hidden file. This mainly affects GNOME, where the file picker portal does not automatically assign file extensions.

### Bug [`1750c5e`](https://github.com/CutWire-Studios/Drift/commit/1750c5e) Property inspector sliders resist mouse drag

Some sliders (especially in audio/video effect properties) resisted dragging from the slider head, forcing users to click the slider track to set the value.

### Bug [`e8f5055`](https://github.com/CutWire-Studios/Drift/commit/e8f5055) / [`10d7d72`](https://github.com/CutWire-Studios/Drift/commit/10d7d72) Timeline media drag-and-drop on Mutter

On Mutter / GNOME, dragging media from the asset library into the timeline caused UI glitches that often dropped the clip onto the first track. Dropping now works reliably, and dropping between, above, or below tracks creates a new track in that position.

### Bug [`4398a9f`](https://github.com/CutWire-Studios/Drift/commit/4398a9f) Wrong waveform range in subtitle cue editor

The waveform shown while editing subtitle cues reflected the wrong portion of the clip.

### Bug [`0f591ee`](https://github.com/CutWire-Studios/Drift/commit/0f591ee) Panels unusable on smaller screens

Asset, properties, subtitle, and text tabs lacked scrolling, so content was clipped on smaller windows. Those panels now scroll.

### Bug [`469f449`](https://github.com/CutWire-Studios/Drift/commit/469f449) Setup project dialog not showing

After the essential-addons startup prompt landed, the project setup / layout chooser dialog stopped appearing. Restored.

### Bug [`1689ff3`](https://github.com/CutWire-Studios/Drift/commit/1689ff3) Addon update notification too intrusive

Addon updates no longer auto-open a blocking dialog. The Extras control in the header pulses when packs or updates need attention instead.

---

## ✨ Added

### Feature [`5082b57`](https://github.com/CutWire-Studios/Drift/commit/5082b57) Aspect-locked clip resize

Resizing video clips in the preview transform overlay keeps aspect ratio by default. Hold Shift to stretch freely. Shape/box clips stay free-form unless Shift is held.

### Feature [`a69ec4b`](https://github.com/CutWire-Studios/Drift/commit/a69ec4b) Edit slider values as text

Double-click (or otherwise open) a property slider to type an exact numeric value.

### Feature [`aebaebe`](https://github.com/CutWire-Studios/Drift/commit/aebaebe) Delete media from the assets panel

Imported clips can be removed from the asset library (closes [#6](https://github.com/CutWire-Studios/Drift/issues/6)).

### Feature [`23c275c`](https://github.com/CutWire-Studios/Drift/commit/23c275c) Audio tempo follows clip speed

Changing a clip’s playback speed also retimes its audio, so sped-up or slowed clips stay in sync in the editor and on export (closes [#8](https://github.com/CutWire-Studios/Drift/issues/8)).

### Feature [`d8aaa8c`](https://github.com/CutWire-Studios/Drift/commit/d8aaa8c) Proxy-based video reverse

Reversing video builds a reverse proxy with a progress dialog instead of relying on the previous on-the-fly path, for more reliable reverse playback and export.

### Feature [`fda9342`](https://github.com/CutWire-Studios/Drift/commit/fda9342) Disable and reorder effects

Effects on a clip can be toggled on/off and moved up/down in the stack (closes [#12](https://github.com/CutWire-Studios/Drift/issues/12)).

### Feature [`e26518a`](https://github.com/CutWire-Studios/Drift/commit/e26518a) Replace media in place

Replace a library asset and keep timeline clips, effects, and layout — a template-style “swap the footage” workflow (closes [#11](https://github.com/CutWire-Studios/Drift/issues/11)).

### Feature [`f10afaf`](https://github.com/CutWire-Studios/Drift/commit/f10afaf) Prompt to install essential addons on startup

On launch, Drift can prompt to install recommended / essential addon packs when they are missing.

### Feature [`e4118d7`](https://github.com/CutWire-Studios/Drift/commit/e4118d7) Timeline bookmarks as jump points

Bookmarks are clearer jump markers: click to seek, drag to reposition, double-click to rename, with a context menu for management.

### Feature [`2126947`](https://github.com/CutWire-Studios/Drift/commit/2126947) Search in the asset panel

Media, effects, templates, shapes, stickers, shortcuts, and related browsers gained a search field.

### Feature [`b9be965`](https://github.com/CutWire-Studios/Drift/commit/b9be965) Rename clips and media assets

Clips on the timeline and media in the asset library can be renamed.

### Feature [`6cc5c0d`](https://github.com/CutWire-Studios/Drift/commit/6cc5c0d) Freeze frame captures the composite image

Freeze frame now saves the full composited frame at the playhead (effects, overlays, and stack), not only the active source video frame. Stills are stored as project media.

### Feature [`2b6fe94`](https://github.com/CutWire-Studios/Drift/commit/2b6fe94) Beauty & makeup face effects

New face effects built on existing face tracking, focused on makeup-style looks (lipstick, blush, eyeliner, and related), alongside the earlier fun face-altering effects.

### Feature [`cc0f73c`](https://github.com/CutWire-Studios/Drift/commit/cc0f73c) Favorites and category rail in asset browsers

Asset browser tabs gained favorites (star) and a category rail for faster browsing of effects, shapes, stickers, and similar libraries.

### Feature [`f993551`](https://github.com/CutWire-Studios/Drift/commit/f993551) Better timeline zoom range and fit-to-view

Timeline can zoom much further out for long projects, ruler ticks scale for multi-hour timelines, and a **Fit timeline in view** control zooms to show the whole project.

---

## 🎨 Improved

### [`d3566fd`](https://github.com/CutWire-Studios/Drift/commit/d3566fd) Text properties panel and style packs

The Text properties panel is grouped into Content, Style, Layout, Appearance, Word accent, and Animation sections. Appearance, accent, and animation start collapsed. Feature toggles (outline, shadow, background, glow, underline, word highlight) use switches that reveal their controls, and outline is a real on/off flag that keeps its width when disabled.

The assets Text tab lists style packs instead of a blank text field. Click a pack to add a styled text clip at the playhead with placeholder copy ready to edit. Style-pack thumbnails use clearer sample phrases and a dark preview canvas so they stay readable in light and dark mode. In the properties Text tab, style packs are a compact dropdown rather than a full grid.

### [`f8018a0`](https://github.com/CutWire-Studios/Drift/commit/f8018a0) Interaction animations and background activity notifications

Visual polish focused on interaction animations and clearer notifications while background work is running.

### [`5366640`](https://github.com/CutWire-Studios/Drift/commit/5366640) Clearer panel tabs, including Subtitles and Audio Effects

Asset and properties tabs are reorganized with dedicated Subtitles and Audio Effects tabs and updated icons.

### [`7273d33`](https://github.com/CutWire-Studios/Drift/commit/7273d33) / [`8c4fc2c`](https://github.com/CutWire-Studios/Drift/commit/8c4fc2c) Favorites layout and layout-picker padding

Favorites presentation in asset browsers and button padding in the layout chooser.

### [`e95f304`](https://github.com/CutWire-Studios/Drift/commit/e95f304) Color adjustments grouped under Color

Brightness, contrast, gamma, hue, saturation, and temperature effects moved into the Color category.

---

## ⚡ Performance

### [`8760e2d`](https://github.com/CutWire-Studios/Drift/commit/8760e2d) Reuse a single filmstrip tile decoder

Filmstrip thumbnail batches reuse one decoder instead of opening a new one per batch.

### [`6ef9225`](https://github.com/CutWire-Studios/Drift/commit/6ef9225) Lighter subtitle voice waveform mixing

Subtitle voice waveforms mix in smaller windows and only while the cue lane is open.
