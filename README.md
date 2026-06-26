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
| Qt | 6.5+ (Quick, QuickControls2, Multimedia, Test, Concurrent, Widgets, OpenGL, Network) |
| FFmpeg | 8.x (libavformat, libavcodec, libavutil, libswscale, libswresample, libavfilter) |
| libzstd | any (addon package decompression) |
| OpenSSL | 3.x, libcrypto only (addon signature verification) |

ONNX Runtime powers auto-subtitles and is downloaded automatically at configure time; pass
`-DDRIFT_FETCH_ONNXRUNTIME=OFF` to use a system install instead.

On Debian/Ubuntu the two new libraries are `libzstd-dev` and `libssl-dev`; on Arch they are
`zstd` and `openssl`. Neither has a download fallback, so configure fails with a pkg-config
error if the development headers are absent.

Optional: OpenCV for future background-removal work (`-DWITH_BGREMOVAL=ON`).
Only `core`, `imgproc`, and `imgcodecs` are linked — not the full OpenCV stack.

**Nothing has to be placed by hand.** Fonts, emoji stickers and the Whisper model used to be
fetched or dropped into the source tree at build time; they are addons now (see below), so a
clone builds and runs with no assets present.

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

Test targets: `Core`, `EditorState`, `Playback`, `Engine`, `MediaProbe`, `AddonPackage`.

`AddonPackage` verifies against a signed fixture in `tests/data/`, so that file has to be
checked out with the repo.

## Addons

Fonts, emoji stickers and speech models are downloaded at runtime rather than built in, which
keeps the binary small and lets a user take only what they need. Open the Addon Manager from the
header (the layers icon), or follow the install prompt in the font picker, the stickers tab or
the auto-subtitle panel.

Packages are `.driftpkg` archives — zstd-compressed, Ed25519-signed, verified before anything is
written into place — installed under `<AppDataLocation>/addons/`. The format, registry and
installer live in `src/engine/AddonPackage.*`, `src/engine/AddonRegistry.*` and
`src/models/AddonManager.*`.

To work against local content instead of downloading, point any of these at a directory; they
take priority over installed addons:

```bash
DRIFT_FONTS_DIR=/path/to/fonts \
DRIFT_STICKERS_DIR=/path/to/stickers \
DRIFT_WHISPER_MODEL_DIR=/path/to/whisper-small \
  ./build/drift
```

Building and publishing addons is a separate concern and lives in its own repository, along with
the Cloudflare Worker that serves them.

### Pointing at a different service

The endpoint and client token are defined once, in `CMakeLists.txt`, and injected as compile
definitions — `src/models/AddonEndpoint.h` only reads them.

```bash
cmake -B build -DDRIFT_ADDON_INDEX_URL=https://addons.example.com/v1/index \
               -DDRIFT_ADDON_CLIENT_TOKEN=your-token

cmake -B build -DDRIFT_ADDON_INDEX_URL=      # build with no addon service at all
```

With the service disabled the manager lists and installs nothing; already-installed, side-loaded
and `DRIFT_*_DIR` content still works, since none of those involve the service.

The token is not a secret — it ships in every binary and is trivially extractable. It exists so
the bucket cannot be crawled or hotlinked, not to protect anything.

Note that these are CMake *cache* variables: changing the default in `CMakeLists.txt` does not
affect an existing build directory, so pass `-D...` again or reconfigure from scratch.

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
tests/            Unit tests (ctest) + tests/data (signed addon fixture)
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
