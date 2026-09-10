#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright 2026 KytyPlus / KytyPS5 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# build_shadps4.sh — build the bundled shadPS4 from source with the
# KytyPlus user-dir patch applied, then copy the resulting binary into the
# KytyPlus install tree.
#
# Why: the prebuilt shadPS4 binaries do not honor SHADPS4_USER_DIR, so the
# unified user/ tree (saves, settings, caches, sys_modules) would silently
# split if shadPS4 is ever launched from a different cwd. Building from
# source with patches/shadps4-user-dir.patch applied closes that gap.
#
# Usage:
#   build_shadps4.sh <windows|linux|macos> <install_dir> [shadps4_ref]
#
#   install_dir  : KytyPlus install prefix (binary is copied here)
#   shadps4_ref  : git ref to build (default: latest release tag)
#
# On failure this script exits non-zero; callers should fall back to the
# prebuilt download so the pipeline never breaks.

set -euo pipefail

PLATFORM="${1:?usage: build_shadps4.sh <windows|linux|macos> <install_dir> [ref]}"
INSTALL_DIR="${2:?usage: build_shadps4.sh <windows|linux|macos> <install_dir> [ref]}"
SHADPS4_REF="${3:-}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PATCH_FILE="$REPO_ROOT/patches/shadps4-user-dir.patch"
WORK_DIR="$RUNNER_TEMP/shadps4-src"

echo "==> build_shadps4: platform=$PLATFORM install=$INSTALL_DIR ref=${SHADPS4_REF:-latest}"

# Resolve the ref to build. Default to the latest release tag so the bundled
# binary tracks upstream releases.
if [[ -z "$SHADPS4_REF" ]]; then
  SHADPS4_REF="$(curl -s https://api.github.com/repos/shadps4-emu/shadPS4/releases/latest \
    | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -n1)"
fi
echo "==> building shadPS4 at ref: $SHADPS4_REF"

rm -rf "$WORK_DIR"
git clone --recursive --depth 1 --branch "$SHADPS4_REF" \
  https://github.com/shadps4-emu/shadPS4.git "$WORK_DIR"
cd "$WORK_DIR"

# Apply the KytyPlus user-dir patch. Fail hard if it no longer applies so we
# notice upstream drift instead of silently shipping an unpatched binary.
git apply --check "$PATCH_FILE"
git apply "$PATCH_FILE"
echo "==> applied $PATCH_FILE"

# Configure. The SDL frontend is the target KytyPlus launches.
CMAKE_ARGS=(-S . -B build -DCMAKE_BUILD_TYPE=Release)
case "$PLATFORM" in
  windows)
    # Use the same clang-cl toolchain as the KytyPlus build when available.
    if command -v clang-cl >/dev/null 2>&1; then
      CMAKE_ARGS+=(-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl)
    fi
    ;;
  linux)
    # shadPS4 recommends Clang 18+; prefer it over the default GCC.
    if command -v clang >/dev/null 2>&1; then
      CMAKE_ARGS+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
    fi
    ;;
  macos)
    # NOTE: shadPS4 requires macOS 26.0 (CMAKE_OSX_DEPLOYMENT_TARGET 26.0).
    # This path is only usable on a macOS 26+ runner; the CI macOS job uses
    # macos-15 and therefore falls back to the prebuilt download.
    ;;
esac

cmake "${CMAKE_ARGS[@]}"
cmake --build build --parallel

# Locate the produced binary and copy it into the install tree. The SDL
# frontend target is named shadps4-sdl on Linux/macOS and shadPS4.exe on
# Windows; match both the target name and the plain name to be robust to
# upstream naming changes.
case "$PLATFORM" in
  windows)
    BIN="$(find build -maxdepth 3 -type f \( -name 'shadPS4.exe' -o -name 'shadps4-sdl.exe' \) | head -n1)"
    test -n "$BIN"
    install -m 755 "$BIN" "$INSTALL_DIR/shadPS4.exe"
    ;;
  linux)
    BIN="$(find build -maxdepth 3 -type f \( -name 'shadps4-sdl' -o -name 'shadps4' \) | head -n1)"
    test -n "$BIN"
    install -m 755 "$BIN" "$INSTALL_DIR/shadps4"
    ;;
  macos)
    BIN="$(find build -maxdepth 3 -type f \( -name 'shadps4-sdl' -o -name 'shadps4' \) | head -n1)"
    test -n "$BIN"
    install -m 755 "$BIN" "$INSTALL_DIR/shadps4"
    ;;
esac

echo "==> installed shadPS4 binary: $BIN -> $INSTALL_DIR"
