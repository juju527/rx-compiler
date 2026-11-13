#!/bin/sh
# Run from any directory. Generated files stay below docs/LLVM.
set -eu
DOC_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DOC_DIR"
mkdir -p build/tmp build/tectonic-cache
export TMPDIR="$DOC_DIR/build/tmp"
export TECTONIC_CACHE_DIR="$DOC_DIR/build/tectonic-cache"
if [ "$#" -gt 1 ]; then
  echo "Usage: sh build.sh [--offline]" >&2
  exit 2
fi
if [ "$#" -eq 1 ]; then
  if [ "$1" = "--offline" ]; then
    set -- --only-cached
  else
    echo "Usage: sh build.sh [--offline]" >&2
    exit 2
  fi
fi
tectonic "$@" --keep-logs --keep-intermediates \
  --outdir build llvm-ir-guide.tex
cp build/llvm-ir-guide.pdf llvm-ir-guide.pdf
printf '%s\n' "$DOC_DIR/llvm-ir-guide.pdf"
