# Unreleased changes

Tracks work done on `main` **since the last public release**. Use this to see what is already fixed or added before filing an issue. Cleared when a new release ships.

**Last released version:** `0.1.2`

---

## ✅ Fixed

### Bug [`b17a209`](https://github.com/CutWire-Studios/Drift/commit/b17a209) File picker creates hidden files on GNOME Flatpak

In the Flatpak release, saving a file without a file extension created a hidden file. This mainly affects GNOME, where the file picker portal does not automatically assign file extensions.

### Bug [`1750c5e`](https://github.com/CutWire-Studios/Drift/commit/1750c5e) Property inspector sliders resist mouse drag

Some sliders (especially in audio/video effect properties) resisted dragging from the slider head, forcing users to click the slider track to set the value.

---

## 🚧 Work in progress

### Bug [`e8f5055`](https://github.com/CutWire-Studios/Drift/commit/e8f5055) Timeline / media drag-and-drop glitches on Mutter

On Mutter / GNOME, dragging media clips from the asset library into the timeline caused UI glitches that often dropped the clip onto the first track. Partially fixed; still being improved.
