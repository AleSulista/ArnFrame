# CutWire Drift

**Drift** is a free, open-source, beginner-friendly desktop video editor built with **Qt 6**, **QML**, and **FFmpeg**.

The UI layer is in place. Backend work follows a phased plan: a unified compositor, microsecond timeline model, threaded decode, and audio-master playback.

## Features (current)

- Multi-track timeline (video, audio, text) with drag-drop, trim, split, ripple, snap
- Project save/load (JSON `.dcut`, version 2 with v1 migration)
- Undo/redo for timeline edits, track mute/hide, and bookmarks
- Preview playback with `FrameCompositor`, `PreviewItem` (QSG texture), and `QAudioSink` pull-mode audio
- Per-clip transforms (position, scale, rotation, opacity) and libavfilter effects in the compositor
- Media thumbnails, filmstrips, and waveform peaks
- Headless CLI tools for probing media and rendering single frames

## Requirements

| Dependency | Version |
|---|---|
| CMake | ≥ 3.21 |
| C++ compiler | C++20 |
| Qt | 6.5+ (Quick, QuickControls2, Multimedia, Test, Concurrent) |
| FFmpeg | 8.x (libavformat, libavcodec, libavutil, libswscale, libswresample, libavfilter) |

Optional: OpenCV (build with `-DWITH_BGREMOVAL=ON` for future background-removal work).

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

Optional background removal:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DWITH_BGREMOVAL=ON
cmake --build build -j$(nproc)
```

## Run

```bash
./build/drift
```

## Test

```bash
cd build
QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

Test targets: `Core`, `EditorState`, `Playback`, `Engine`, `MediaProbe`.

## CLI tools

Built under `build/tools/`:

```bash
# Probe a media file
./build/tools/probe /path/to/video.mp4

# Render one composited frame from a saved project
./build/tools/renderframe project.dcut.json 1000000 out.png
```

Arguments for `renderframe`: `<project.json> <time_us> <output.png>`.

## Project layout

```
src/
  core/           Domain model (Project, Track, Clip, Keyframe, Effect) — no GUI
  engine/         FFmpeg: ClipReader, FrameCompositor, AudioMixer, EffectProcessor
  models/         QML-facing models: AppController, AssetLibrary, TimelineModel, ClipListModel
  playback/       PlaybackEngine, PlaybackClock, CompositorService
  preview/        PreviewItem (QQuickItem → QSGTexture)
  qml/            UI panels and components
tests/            Unit tests (ctest)
tools/            Headless probe + renderframe
cmake/            FindFFmpeg.cmake
```

### CMake targets

| Target | Role |
|---|---|
| `driftcore` | Core domain + JSON persistence |
| `driftengine` | FFmpeg decode, compositing, effects |
| `drift` | Qt Quick application |

## Architecture (summary)

**Unified frame server** — Preview and (future) export share `FrameCompositor`:

> “Give me the composited RGBA frame + mixed audio at timeline time T (µs).”

**Time model** — All core timeline positions are `int64_t` microseconds (`drift::TimeUs`). QML uses seconds at the boundary via `AppController`.

**Threading**

| Thread | Responsibility |
|---|---|
| Main (GUI) | QML, models, undo stack, playhead UI (~60 Hz) |
| Decode workers | `ClipReaderPool` — one thread per active media path |
| Compositor | `CompositorService` — triple-buffered frames off the GUI thread |
| Audio (pull) | `QAudioSink` → `PlaybackClock` (audio-master) |

**Data flow (video)**

```
Media file → ClipReader → EffectProcessor (libavfilter)
          → FrameCompositor (transforms, blending, text)
          → PreviewItem (QSGTexture)  |  Exporter (planned)
```

**Data flow (audio)**

```
Media file → ClipReader → AudioMixer (keyframed volume)
          → QAudioSink  |  Exporter (planned)
```

See [AGENTS.md](AGENTS.md) for full architecture, phase status, and contributor/agent guidelines.

## Development phases

| Phase | Focus | Status |
|---|---|---|
| **1** | Core model, JSON persistence, undo, models | Complete |
| **2** | Timeline editing, inspector, save/load | Complete |
| **3** | Playback, preview, threaded decode, A/V sync | Complete |
| **4** | Keyframing UI, effect presets, text styling | Not started |
| **5** | Export via `FrameCompositor`, waveforms, fades | Partial (`TimelineExporter` uses ffmpeg CLI) |
| **6** | GPU compositing, packaging, advanced effects | Not started |

**MVP** (Phase 5): Import → multi-track edit → export matching preview.

## QML entry points

Singletons registered in `main.cpp`:

- `EditorState` / `AppController` — timeline controller
- `AssetLibrary` — media bin

## License

GPLv3 — see [LICENSE](LICENSE).
