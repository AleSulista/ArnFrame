# Agent access (Drift MCP)

Drift can expose a localhost MCP server so Cursor, Claude Code, or other agents can edit the open project. Enable it in **Settings → Agent access** (off at every launch).

## Connect

**Cursor / Claude (this session):** copy the snippet from Settings after enabling. The bearer token rotates each session.

**One-time stdio attach** (no token in `mcp.json`):

```json
{
  "mcpServers": {
    "drift": {
      "command": "/path/to/drift",
      "args": ["--mcp-stdio"]
    }
  }
}
```

`drift --mcp-stdio` attaches to a running editor with Agent access enabled.

## Workflow

1. **`catalog`** — toolboxes, per-op “when” hints, endpoints, units (no schemas).
2. **`toolbox({name})`** — full JSON schemas for ops in that toolbox.
3. **`apply({ops:[{tool, args}, …]})`** — run mutations in order; one undo step for the batch.
4. **`inspect({clips:true})`** — project state and stable clip UUIDs. Pass `since:<revision>` from a prior inspect to skip the payload when nothing changed.
5. **`capture()`** — JPEG still of the composition (use to verify edits).

Homepage endpoint: `POST /mcp` with `Authorization: Bearer <token>`.

Pinned endpoints (`/mcp/timeline`, `/mcp/project`, …) list that toolbox’s ops directly. `catalog`, `toolbox`, and `apply` are only on `/mcp`.

## Conventions

| Topic | Rule |
|-------|------|
| Time | Seconds |
| Clip reference | Prefer `clip` UUID from `inspect`; else `track` (0 = top) + `index` |
| Overlap | Off by default — place/move snap to gaps unless `set_overlap` enables overlap |
| Export | `export_video` is async — poll `inspect.export` or `export_status` |
| Change detection | Every `inspect` includes `revision`; pass `since:<revision>` to get `{unchanged:true}` when state is current |

## Toolbox reference

| Toolbox | When to use |
|---------|-------------|
| `media` | Import and inspect the media bin before placing clips |
| `timeline` | Tracks, place/move/trim/split/delete, overlap, undo |
| `canvas` | On-screen position, size, rotation, opacity |
| `playback` | Seek, play, pause, In/Out work area |
| `text` | Title and caption clips |
| `effects` | Video/audio effects and transitions |
| `project` | Canvas size, background, metadata, save, export |

## Example

Import, place, trim, and capture in one batch:

```json
{
  "ops": [
    {"tool": "import_media", "args": {"paths": ["/path/to/clip.mp4"]}},
    {"tool": "place_clip", "args": {"asset": "0", "at": 0}},
    {"tool": "set_duration", "args": {"clip": "<uuid from inspect>", "duration": 5}},
    {"tool": "capture", "args": {}}
  ]
}
```

(`capture` is a homepage tool — call it directly or after `apply` on `/mcp`.)

## Limitations

Not available via MCP yet: keyframes, subtitles, segmentation, speed curves, editor UI preferences (theme, shortcuts).

## Security

- Binds to `127.0.0.1` only.
- Any local process with the session token has full editor access.
- Turn Agent access off when finished.

**Flatpak:** host file import may need `flatpak override --filesystem=home org.cutwire.Drift`.
