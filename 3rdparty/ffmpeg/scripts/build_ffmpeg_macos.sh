#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"

FFMPEG_VERSION="${FFMPEG_VERSION:-8.1}"
ARCH="${ARCH:-$(uname -m)}"

case "$ARCH" in
    arm64|aarch64)
        TARGET_DIR="$PROJECT_DIR/3rdparty/ffmpeg/macos-arm64"
        ;;
    x86_64)
        TARGET_DIR="$PROJECT_DIR/3rdparty/ffmpeg/macos-x64"
        ;;
    *)
        echo "Unsupported macOS architecture: $ARCH" >&2
        exit 1
        ;;
esac

BUILD_ROOT="$PROJECT_DIR/3rdparty/ffmpeg/_build"
SRC_ROOT="$PROJECT_DIR/3rdparty/ffmpeg/_src"
TARBALL_NAME="ffmpeg-${FFMPEG_VERSION}.tar.xz"
TARBALL_PATH="$SRC_ROOT/$TARBALL_NAME"
SOURCE_DIR="$SRC_ROOT/ffmpeg-${FFMPEG_VERSION}"
DOWNLOAD_URL="https://ffmpeg.org/releases/${TARBALL_NAME}"

mkdir -p "$BUILD_ROOT" "$SRC_ROOT" "$TARGET_DIR/licenses"

if [[ ! -f "$TARBALL_PATH" ]]; then
    echo "Downloading ${DOWNLOAD_URL}"
    curl -L "$DOWNLOAD_URL" -o "$TARBALL_PATH"
fi

if [[ ! -d "$SOURCE_DIR" ]]; then
    echo "Extracting ${TARBALL_NAME}"
    tar -xf "$TARBALL_PATH" -C "$SRC_ROOT"
fi

cd "$SOURCE_DIR"

echo "Configuring FFmpeg ${FFMPEG_VERSION} for ${ARCH}"
./configure \
    --prefix="$TARGET_DIR" \
    --arch="$ARCH" \
    --enable-shared \
    --disable-static \
    --disable-programs \
    --disable-doc \
    --disable-debug \
    --enable-avformat \
    --enable-avcodec \
    --enable-avutil \
    --enable-swscale \
    --enable-swresample

echo "Building FFmpeg"
make -j"$(sysctl -n hw.ncpu)"

echo "Installing FFmpeg into $TARGET_DIR"
make install

cp COPYING.LGPLv2.1 "$TARGET_DIR/licenses/" 2>/dev/null || true
cp COPYING.LGPLv3 "$TARGET_DIR/licenses/" 2>/dev/null || true
cp LICENSE.md "$TARGET_DIR/licenses/" 2>/dev/null || true

echo
echo "Private FFmpeg is ready in:"
echo "  $TARGET_DIR"
echo
echo "Re-run CMake so this project picks it up before Homebrew."
