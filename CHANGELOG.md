# Unreleased changes

Tracks work done on `main` **since the last public release**. Use this to see what is already fixed or added before filing an issue. Cleared when a new release ships.

**Last released version:** `0.3.0`

---

## ✅ Fixed

### Bug [`cc20353`](https://github.com/CutWire-Studios/Drift/commit/cc20353) NVIDIA on Wayland

The preview stayed black on NVIDIA under Wayland (and earlier builds fell back to X11 to work around it). Drift now requests a desktop OpenGL 3.3 context that the compositor can share, so NVIDIA Wayland runs as a native Wayland app with a working preview — including Flatpak (closes [#41](https://github.com/CutWire-Studios/Drift/issues/41)).

---

## ✨ Added

### Feature (#70) Remember window geometry and panel sizes

The editor now reopens the way you left it. Window size, position and whether it was maximized are remembered between sessions, as are the panel proportions you drag out — the preview/timeline split and the widths of the assets and properties panels. The portrait and landscape workspaces each keep their own arrangement. Window position is not restored on Wayland, where the compositor, not the app, decides where a window opens.

### Feature (#73) Close gap and split one clip

Right-click an empty gap on a track and choose **Close Gap** to pull every later clip on that track left, keeping linked video/audio partners in step. Undo restores the gap. A clip’s context menu now has **Split item at current time**, which cuts only that clip (and its linked partner) instead of every clip under the playhead.

### Feature [`cdbe317`](https://github.com/CutWire-Studios/Drift/commit/cdbe317) Hardware or software decode for preview

The preview toolbar has an **Auto / Software / Hardware** decode picker. Auto uses hardware for heavy 4K clips and the CPU otherwise; Software is smoother for most clips; Hardware is better for high-quality 4K. AV1 preview picks a hardware-capable decoder when one is available. If playback stutters, try another mode.

### Feature Hardware video decoding on Windows and macOS

Preview decode used to reach the GPU only on Linux, through VAAPI. It now probes a per-platform list: **NVDEC then Direct3D 11** on Windows, **VideoToolbox** on macOS, and **NVDEC then VAAPI** on Linux — so an NVIDIA machine no longer depends on the VAAPI driver for decode.

The preview decode picker names the backend instead of offering a single **Hardware** entry: **Auto**, **Software**, then one entry per backend that actually opens on your machine (for example **Hardware (NVDEC)** and **Hardware (VAAPI)** on an NVIDIA Linux box). Picking one uses it for every clip and never quietly switches to another, so if a driver is the problem you can pick around it. A saved choice naming a backend the machine does not have falls back to Auto rather than to a mode that would never engage.

Hardware decode that fails mid-playback used to be silent — the reader dropped to software on its own and the preview just got slower. It now says so, and the debug report gained an **Active decode** line showing what the preview actually ended up using. The `DRIFT_NO_VAAPI` escape hatch is now `DRIFT_NO_HWACCEL`, and it also stops the startup device probe.

### Feature (#40) Hardware video encoding on export

The export dialog’s **Video encoder** list now includes GPU encoders when the machine can run them: NVIDIA NVENC, Intel Quick Sync, AMD AMF, VAAPI on Linux, and VideoToolbox on macOS, for H.264, H.265 and AV1. Unavailable backends are omitted; picking one that is listed actually encodes on the GPU.

### Feature [`37cd702`](https://github.com/CutWire-Studios/Drift/commit/37cd702) Debug info for bug reports

A bug button in the header opens **Debug info**: software and hardware decoder/encoder support, host facts, and on Flatpak which extensions are mounted (and which to install if a codec is missing). **Copy report** puts a paste-ready snapshot on the clipboard for a GitHub issue.

### Feature (#81) Save and share text styles

The Text inspector can **Save style…** from the look of the selected clip. Saved styles appear under **My styles** in the Text assets tab, and can be renamed, deleted, exported as `.drifttextstyle`, or imported so the same titles travel between projects.

### Feature (#66) Subtitle editor and emoji in text

The subtitle editor scrolls and highlights the cue on screen as the playhead moves, and you edit in a fixed panel below so the list always shows neighbouring lines. Titles and captions can include colour emoji when the emoji-font add-on is installed from Extras — paste them in, and they draw instead of disappearing.

### Feature (#60) Save project as JSON

**Save as JSON…** in the project menu writes a human-readable `.json` copy of the open project. It is an export, not a substitute for the `.drift` project file: the current project path is left alone.

---

## 🎨 Improved

### [`586634c`](https://github.com/CutWire-Studios/Drift/commit/586634c) CUDA add-on on Windows

Windows loads ONNX Runtime from the addon's own folder. CUDA/cuDNN DLLs bundled next to the NVIDIA AI Engine are found there, so a CUDA Toolkit on PATH is no longer required.

### [`70b9f60`](https://github.com/CutWire-Studios/Drift/commit/70b9f60) Faster media imports, less memory

The media library only builds thumbnails for assets that are on screen, and still-image thumbnails decode at the size they will be shown rather than full resolution. Large bins (timelapses, photo dumps) import faster and sit at a few hundred megabytes instead of well over a gigabyte.
