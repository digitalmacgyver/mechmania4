#!/bin/bash
# Build MechMania IV from vendored dependencies
# No internet connection required!

set -e

VENDOR_DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${PREFIX:-$VENDOR_DIR/local}"
BUILD_DIR="${BUILD_DIR:-$VENDOR_DIR/build-tmp}"

echo "Building MechMania IV from vendored dependencies..."
echo "No internet connection required!"
echo "Install prefix: $PREFIX"

mkdir -p "$BUILD_DIR"
mkdir -p "$PREFIX"

build_dep() {
  local archive="$1"
  local dir_name
  dir_name=$(tar -tzf "$archive" | head -1 | cut -f1 -d"/")
  rm -rf "$BUILD_DIR/$dir_name"
  tar -xzf "$archive" -C "$BUILD_DIR"
  pushd "$BUILD_DIR/$dir_name" >/dev/null
  ./configure --prefix="$PREFIX"
  make -j"$(nproc)"
  make install
  popd >/dev/null
}

build_dep "$VENDOR_DIR/source-packages/SDL2-"*.tar.gz
build_dep "$VENDOR_DIR/source-packages/SDL2_image-"*.tar.gz
build_dep "$VENDOR_DIR/source-packages/SDL2_ttf-"*.tar.gz
build_dep "$VENDOR_DIR/source-packages/libmodplug-"*.tar.gz

# Ensure pkg-config can locate newly installed libraries when building mixer
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
build_dep "$VENDOR_DIR/source-packages/SDL2_mixer-"*.tar.gz

touch "$PREFIX/.mm4_vendor_complete"
rm -rf "$BUILD_DIR"

echo "SDL libraries installed under $PREFIX"
echo "Add this to your environment before running cmake:"
echo "  export PKG_CONFIG_PATH=\"$PREFIX/lib/pkgconfig:\$PKG_CONFIG_PATH\""
echo "  export CMAKE_PREFIX_PATH=\"$PREFIX:\$CMAKE_PREFIX_PATH\""

echo "Dependencies installed! Now build MechMania IV:"
echo "  cd /path/to/mechmania4"
echo "  cmake -S . -B build"
echo "  make -j$(nproc)"
