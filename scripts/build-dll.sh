#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Eike Hein
# SPDX-License-Identifier: MIT

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="$ROOT/third_party/ffmpeg-i686"
SYSROOT="/usr/i686-w64-mingw32/sys-root/mingw"
OUT="$ROOT/dist/smackw32.dll"

if [[ ! -f "$PREFIX/lib/pkgconfig/libavformat.pc" ]]; then
    echo "FFmpeg is not built yet. Run ./scripts/build-ffmpeg.sh first." >&2
    exit 1
fi

mkdir -p "$ROOT/dist"

export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$SYSROOT/lib/pkgconfig"
export PKG_CONFIG_PATH=

COMMON_CFLAGS=(
    -std=c17
    -O3
    -Wall
    -Wextra
    -Wno-unused-parameter
    -D_WIN32_WINNT=0x0501
    -DWINVER=0x0501
    -mstackrealign
    -mpreferred-stack-boundary=4
    -mincoming-stack-boundary=2
    -I"$ROOT/src"
)

FFMPEG_CFLAGS=($(pkg-config --cflags libavformat libavcodec libswscale libswresample libavutil))
FFMPEG_LIBS=($(pkg-config --static --libs libavformat libavcodec libswscale libswresample libavutil))
FILTERED_FFMPEG_LIBS=()
for lib in "${FFMPEG_LIBS[@]}"; do
    case "$lib" in
        -latomic|-lbcrypt|-pthread|-lpthread)
            ;;
        *)
            FILTERED_FFMPEG_LIBS+=("$lib")
            ;;
    esac
done

i686-w64-mingw32-gcc \
    "${COMMON_CFLAGS[@]}" \
    "${FFMPEG_CFLAGS[@]}" \
    -shared \
    -o "$OUT" \
    "$ROOT/src/smackshim.c" \
    "$ROOT/src/smackw32.def" \
    -Wl,--enable-stdcall-fixup \
    -Wl,--no-insert-timestamp \
    -Wl,--gc-sections \
    -Wl,--exclude-all-symbols \
    -static \
    -static-libgcc \
    -Wl,-Bstatic \
    "${FILTERED_FFMPEG_LIBS[@]}" \
    -lwinpthread \
    -lz \
    -Wl,-Bdynamic \
    -lwinmm \
    -lole32 \
    -luser32 \
    -lgdi32 \
    -ldsound

i686-w64-mingw32-strip --strip-unneeded "$OUT"

echo "Built $OUT"
