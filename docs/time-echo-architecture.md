# Time Echo / Trail — architecture (v1)

## Goal

Blend previous frames of a clip to create motion trails (`time_echo` preset).

## Constraint: single compositor path

Preview and export both call `FrameCompositor::compositeAt()`. Time echo must not introduce a
second compositing pipeline or a global preview-only history buffer.

## v1 approach: per-clip temporal re-sampling

`time_echo` is **not** a single-input `CompositorEffects` pixel shader. It needs multiple
decoded frames of the same clip at earlier timeline offsets.

Implementation lives in `FrameCompositor::imageForClip()`:

1. Decode the current clip frame (no effects).
2. When `time_echo` is in the clip effect stack, decode up to `frames` prior samples at
   `clipTimeUs - n * frameDurationUs(project.fps())` (clamped to clip bounds).
3. Blend samples with `CompositorFrameHistory::applyTimeEcho()` (deterministic decay weighting).
4. Apply the remaining clip effects via `EffectProcessor` (excluding `time_echo`).

Historical samples are decoded **without** `time_echo` to avoid recursive trails.

## Determinism

Trail timing uses integer microsecond steps from `drift::frameDurationUs(fps)`. The same
project, clip, timeline time, and parameters produce identical output in preview and export.

## Parameters

| Key | Range | Default | Notes |
|-----|-------|---------|-------|
| `frames` | 1–10 | 4 | Count of prior project frames to sample |
| `decay` | 0–1 | 0.55 | Opacity falloff per frame age (`decay^age`) |
| `blendMode` | normal / add / screen | normal | Echo layer composite mode |

## Future extensions

- **Project-level echo** (trails across stacked clips): would need compositor output history
  after full canvas composite; not required for per-clip motion trails.
- **Sub-frame spacing**: optional clip-speed-aware step or shutter-angle control.
- **Static image clips**: v1 samples identical frames; trails only appear on video/moving content.
