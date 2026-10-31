#!/usr/bin/env bash
# Disassembles each HyperScan demo's HYPER.EXE into build/disasm/<demo>.s using tools/sdis.
# Run from anywhere: scripts/disasm_demos.sh [demos-dir]
# Load base 0xA00901FC, code starts at 0xA0091000 (see docs/phase4a-conformance-fix-plan.md).
# Output is git-ignored (build/disasm/) and must not be committed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEMOS="${1:-$ROOT/ROMs/HS demos}"
OUT="$ROOT/build/disasm"
BASE=0xA00901FC
START=0xA0091000

SDIS="$ROOT/tools/sdis/sdis"
[ -x "$SDIS" ] || [ -x "$SDIS.exe" ] || (cd "$ROOT/src" && make -f build.mk sdis)

[ -d "$DEMOS" ] || { echo "disasm_demos: demos directory not found: $DEMOS" >&2; exit 1; }
mkdir -p "$OUT"

count=0
for dir in "$DEMOS"/*/; do
    exe="$dir/HYPER.EXE"
    [ -f "$exe" ] || continue
    name="$(basename "$dir")"
    out="$OUT/${name// /_}.s"
    size=$(wc -c < "$exe")
    end=$(printf '0x%X' $((BASE + size)))
    "$SDIS" "$exe" "$BASE" "$START" "$end" > "$out"
    echo "$name -> ${out#"$ROOT"/} ($(wc -l < "$out") lines)"
    count=$((count + 1))
done

[ "$count" -gt 0 ] || { echo "disasm_demos: no HYPER.EXE found under $DEMOS" >&2; exit 1; }
