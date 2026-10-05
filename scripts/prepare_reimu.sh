#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
reimu_dir="$repo_root/vendor/REIMU"
patch_file="$repo_root/patches/reimu-libcxx.patch"

if [ ! -f "$reimu_dir/include/assembly/forward.h" ]; then
    echo "REIMU is not initialized; run git submodule update --init --recursive first." >&2
    exit 1
fi

if git -C "$reimu_dir" apply --reverse --check "$patch_file" 2>/dev/null; then
    echo "REIMU compatibility patch is already applied."
else
    git -C "$reimu_dir" apply "$patch_file"
    echo "Applied REIMU compatibility patch."
fi
