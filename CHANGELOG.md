# Unreleased changes

Tracks work done on `main` **since the last public release**. Use this to see what is already fixed or added before filing an issue. Cleared when a new release ships.

**Last released version:** `0.3.0`

---

## ✅ Fixed

## ✨ Added

- The editor now reopens the way you left it. Window size, position and whether it was maximized are remembered between sessions, as are the panel proportions you drag out — the preview/timeline split and the widths of the assets and properties panels. The portrait and landscape workspaces each keep their own arrangement. Window position is not restored on Wayland, where the compositor, not the app, decides where a window opens.

## 🎨 Improved

- Windows loads ONNX Runtime from the addon's own folder. CUDA/cuDNN DLLs bundled next to the NVIDIA AI Engine are found there, so a CUDA Toolkit on PATH is no longer required.
