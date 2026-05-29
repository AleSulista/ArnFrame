# File-based GPU effects

All effect presets are GPU packages under `effects/` (`backend: "gpu"`). Preview and export share `FrameCompositor` → `EffectProcessor` → `GpuEffectExecutor`.

## Package layout

```
effects/
└── adjust_contrast/
    ├── effect.json
    └── main.frag
```

Search order: `DRIFT_EFFECTS_DIR`, `<applicationDir>/effects`, `<AppDataLocation>/effects`.

## effect.json

| Field | Meaning |
|---|---|
| `id` / `displayName` / `category` / `order` | Catalog metadata |
| `backend` | `"gpu"` |
| `parameters[]` | Slider/bool uniforms |
| `fixedParams` | Hidden uniforms (colors as `#rrggbb`, enums as strings) |
| `pipeline` | `intermediateBuffers` + `passes` |

Pass inputs: `source_texture` or `buffer` (+ `id`). Multiple inputs bind as `u_currentTexture` (unit 0) and `u_texture1`…  
Pass outputs: `buffer` or `canvas`.

## GLSL

- `#version 330 core`
- Reserved: `u_currentTexture`, `u_textureN`, `u_resolution`, `u_time`, `u_timeUs`, `u_frameIndex`

**Grace mode:** compile/GL failure → passthrough.

## Special case: time_echo

History frames are still decoded in `FrameCompositor`; blending runs on the GPU via `GpuEffectExecutor::blendTimeEcho` (CPU fallback if GL is unavailable).
