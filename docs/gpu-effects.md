# File-based GPU effects

All effect presets are GPU packages under `effects/` (`backend: "gpu"`). Preview and export share `FrameCompositor` → `EffectProcessor` → `GpuEffectExecutor`.

## Package layout

```
effects/
└── adjust_contrast/
    ├── effect.json
    ├── thumbnail.png   # optional browser preview (auto-picked if present)
    └── main.frag
```

Search order: `DRIFT_EFFECTS_DIR`, `<applicationDir>/effects`, `<AppDataLocation>/effects`.

## effect.json

| Field | Meaning |
|---|---|
| `id` / `displayName` / `category` / `order` | Catalog metadata |
| `thumbnail` | Optional image path (relative to package, or absolute). Defaults to `thumbnail.png` when that file exists |
| `backend` | `"gpu"` |
| `parameters[]` | Slider/bool uniforms |
| `fixedParams` | Hidden uniforms (colors as `#rrggbb`, enums as strings) |
| `pipeline` | `intermediateBuffers` + `passes` |

Pass inputs: `source_texture` (+ optional `index`), `buffer` (+ `id`), or `texture` (+ `id`). Multiple inputs bind as `u_currentTexture` (unit 0) and `u_texture1`…  
Pass outputs: `buffer` or `canvas`.

`pipeline.textures[]` declares static image assets loaded once from the package dir:

```json
"textures": [{ "id": "glyphs", "file": "glyphs.png" }]
```

## GLSL

- `#version 330 core`
- Reserved: `u_currentTexture`, `u_textureN`, `u_resolution`, `u_time`, `u_timeUs`, `u_frameIndex`, `u_progress`, `u_fromTexture`, `u_toTexture`

**Grace mode:** compile/GL failure → passthrough.

## Special case: time_echo

History frames are still decoded in `FrameCompositor`; blending runs on the GPU via `GpuEffectExecutor::blendTimeEcho` (CPU fallback if GL is unavailable).
