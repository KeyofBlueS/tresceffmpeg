#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Eike Hein
# SPDX-License-Identifier: MIT

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THIRD_PARTY="$ROOT/third_party"
BUILD="$ROOT/build/ffmpeg-i686"
PREFIX="$THIRD_PARTY/ffmpeg-i686"
FFMPEG_VERSION="${FFMPEG_VERSION:-8.1.1}"
ARCHIVE="ffmpeg-${FFMPEG_VERSION}.tar.xz"
URL="https://ffmpeg.org/releases/${ARCHIVE}"
SYSROOT="/usr/i686-w64-mingw32/sys-root/mingw"

mkdir -p "$THIRD_PARTY" "$BUILD"

if [[ ! -f "$THIRD_PARTY/$ARCHIVE" ]]; then
    curl -L "$URL" -o "$THIRD_PARTY/$ARCHIVE"
fi

if [[ ! -d "$THIRD_PARTY/ffmpeg-${FFMPEG_VERSION}" ]]; then
    tar -C "$THIRD_PARTY" -xf "$THIRD_PARTY/$ARCHIVE"
fi

perl -0pi -e 's|#if HAVE_BCRYPT|#if 0|g' \
    "$THIRD_PARTY/ffmpeg-${FFMPEG_VERSION}/libavutil/random_seed.c"

rm -rf "$BUILD" "$PREFIX"
mkdir -p "$BUILD" "$PREFIX"

export PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig"
export PKG_CONFIG_PATH=

cd "$BUILD"

"$THIRD_PARTY/ffmpeg-${FFMPEG_VERSION}/configure" \
    --prefix="$PREFIX" \
    --pkg-config=pkg-config \
    --pkg-config-flags=--static \
    --enable-cross-compile \
    --cross-prefix=i686-w64-mingw32- \
    --target-os=mingw32 \
    --arch=x86_32 \
    --cpu=i686 \
    --extra-cflags="-D_WIN32_WINNT=0x0501 -DWINVER=0x0501 -DWC_ERR_INVALID_CHARS=0 -O3 -mstackrealign -mpreferred-stack-boundary=4 -mincoming-stack-boundary=2" \
    --extra-ldflags="-static" \
    --disable-shared \
    --enable-static \
    --disable-programs \
    --disable-doc \
    --disable-debug \
    --disable-everything \
    --disable-inline-asm \
    --disable-ssse3 \
    --disable-sse4 \
    --disable-sse42 \
    --disable-avx \
    --disable-avx2 \
    --disable-avx512 \
    --disable-avx512icl \
    --disable-xop \
    --disable-fma3 \
    --disable-fma4 \
    --disable-aesni \
    --disable-clmul \
    --disable-network \
    --disable-w32threads \
    --enable-pthreads \
    --disable-os2threads \
    --disable-bzlib \
    --disable-autodetect \
    --disable-avdevice \
    --disable-avfilter \
    --disable-encoders \
    --disable-muxers \
    --disable-devices \
    --disable-protocols \
    --enable-protocol=file \
    --enable-demuxer=avi,matroska,mov,mp3,ogg,smacker,wav \
    --enable-decoder=aac,h264,hevc,mp3float,opus,pcm_s16be,pcm_s16le,pcm_u8,smackaud,smacker,theora,vorbis,vp8,vp9 \
    --enable-parser=aac,h264,hevc,mpegaudio,opus,vorbis,vp8,vp9 \
    --enable-avcodec \
    --enable-avformat \
    --enable-avutil \
    --enable-swscale \
    --enable-swresample \
    --enable-zlib \
    --enable-runtime-cpudetect

sed -i \
    -e 's/^#define HAVE_CLOCK_GETTIME .*/#define HAVE_CLOCK_GETTIME 0/' \
    -e 's/^#define HAVE_NANOSLEEP .*/#define HAVE_NANOSLEEP 0/' \
    config.h

make -j"$(nproc)"
make install
