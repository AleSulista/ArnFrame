#!/usr/bin/env bash
# Sync Lucide icons used by Drift from a local lucide-icons checkout.
# Copies SVG sources and rasterises 48px PNGs for QML (Qt Image has no SVG decoder here).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${LUCIDE_ICONS_DIR:-$ROOT/../lucide-icons-1.25.0/icons}"
DEST="$ROOT/resources/icons"

if [[ ! -d "$SRC" ]]; then
  echo "Lucide icons not found at: $SRC" >&2
  echo "Set LUCIDE_ICONS_DIR to the icons/ directory of lucide-icons." >&2
  exit 1
fi

if ! command -v rsvg-convert >/dev/null 2>&1; then
  echo "rsvg-convert is required to build PNG icons." >&2
  exit 1
fi

mkdir -p "$DEST"
icons=(
  scissors chevrons-left undo redo clipboard-paste copy-plus copy trash-2 snowflake
  bookmark layers magnet link-2 unlink-2 fold-horizontal zoom-out zoom-in gauge play pause
  maximize folder headphones type smile wand-sparkles sliders-horizontal settings upload plus
  volume-2 volume-off eye eye-off film music image shapes chevron-down chevrons-right x
  message-square moon sun grid-3x3 list arrow-down-a-z grip-vertical
  save arrow-right-to-line arrow-left-to-line tags blend square-dashed puzzle info
  # Project bundle: package/properties entries in the project menu
  package file-text
  # Status / feedback (toasts, inline errors, empty states)
  triangle-alert circle-check circle-x loader-circle refresh-cw download
  # Affordances (reset, search, disclosure, editing, locking)
  rotate-ccw search chevron-right chevron-left check pencil clock lock lock-open
  # Text alignment controls (properties panel)
  text-align-start text-align-center text-align-end
  align-start-vertical align-center-vertical align-end-vertical
  # Timeline / media
  captions list-video move-horizontal audio-lines video
  # Shortcuts tab and canvas crop tool
  keyboard crop minimize
)

for name in "${icons[@]}"; do
  cp "$SRC/$name.svg" "$DEST/$name.svg"
  sed 's/stroke="currentColor"/stroke="#ffffff"/g' "$DEST/$name.svg" \
    | rsvg-convert -w 48 -h 48 -f png -o "$DEST/$name.png"
done

echo "Synced ${#icons[@]} icons (SVG + PNG) to $DEST"
