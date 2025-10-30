#!/bin/bash
# Download and vendor all dependencies for long-term preservation
# This ensures MechMania IV can be built even if upstream sources disappear

set -e

VENDOR_DIR="$(dirname "$0")"
cd "$VENDOR_DIR"

echo "MechMania IV Dependency Preservation Script"
echo "==========================================="
echo "Downloading all dependencies for offline builds..."

# Create directories
mkdir -p source-packages
mkdir -p docker-images
mkdir -p binaries

# SDL2 Libraries
echo -e "\n[1/4] Downloading SDL2 libraries..."
SDL2_VERSION="2.28.5"
SDL2_IMAGE_VERSION="2.8.2"
SDL2_TTF_VERSION="2.22.0"
SDL2_MIXER_VERSION="2.6.3"
LIBMODPLUG_VERSION="0.8.9.0"

if [ ! -f "source-packages/SDL2-${SDL2_VERSION}.tar.gz" ]; then
    wget -P source-packages/ "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz"
fi

if [ ! -f "source-packages/SDL2_image-${SDL2_IMAGE_VERSION}.tar.gz" ]; then
    wget -P source-packages/ "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image-${SDL2_IMAGE_VERSION}.tar.gz"
fi

if [ ! -f "source-packages/SDL2_ttf-${SDL2_TTF_VERSION}.tar.gz" ]; then
    wget -P source-packages/ "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf-${SDL2_TTF_VERSION}.tar.gz"
fi

if [ ! -f "source-packages/SDL2_mixer-${SDL2_MIXER_VERSION}.tar.gz" ]; then
    wget -P source-packages/ "https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer-${SDL2_MIXER_VERSION}.tar.gz"
fi

if [ ! -f "source-packages/libmodplug-${LIBMODPLUG_VERSION}.tar.gz" ]; then
    wget -P source-packages/ "https://downloads.sourceforge.net/project/modplug-xmms/libmodplug/${LIBMODPLUG_VERSION}/libmodplug-${LIBMODPLUG_VERSION}.tar.gz"
fi

# CMake (for building)
echo -e "\n[2/4] Downloading CMake..."
CMAKE_VERSION="3.27.9"
if [ ! -f "source-packages/cmake-${CMAKE_VERSION}.tar.gz" ]; then
    wget -P source-packages/ "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}.tar.gz"
fi

# Docker base images
echo -e "\n[3/4] Saving Docker base images..."
echo "Pulling and saving base images for offline use..."

# Save Alpine image
if command -v docker &> /dev/null; then
    docker pull alpine:3.18
    docker save alpine:3.18 | gzip > docker-images/alpine-3.18.tar.gz

    docker pull ubuntu:22.04
    docker save ubuntu:22.04 | gzip > docker-images/ubuntu-22.04.tar.gz
else
    echo "Docker not available, skipping base image download"
fi

# Create checksums
echo -e "\n[4/4] Creating checksums..."
cd source-packages
sha256sum *.tar.gz > SHA256SUMS
cd ..

echo -e "\nCreating offline build script..."
cat > build-offline.sh <<'EOF'
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

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="$PREFIX/lib:${LD_LIBRARY_PATH:-}"

build_dep() {
  local archive=$1
  shift
  local configure_args=("$@")
  local dir_name
  dir_name=$(tar -tzf "$archive" | head -1 | cut -f1 -d"/")
  rm -rf "$BUILD_DIR/$dir_name"
  tar -xzf "$archive" -C "$BUILD_DIR"
  pushd "$BUILD_DIR/$dir_name" >/dev/null
  ./configure --prefix="$PREFIX" "${configure_args[@]}"
  make -j"$(nproc)"
  make install
  popd >/dev/null
}

build_dep "$VENDOR_DIR/source-packages/SDL2-"*.tar.gz
build_dep "$VENDOR_DIR/source-packages/SDL2_image-"*.tar.gz
build_dep "$VENDOR_DIR/source-packages/SDL2_ttf-"*.tar.gz
build_dep "$VENDOR_DIR/source-packages/libmodplug-"*.tar.gz --disable-dependency-tracking
build_dep "$VENDOR_DIR/source-packages/SDL2_mixer-"*.tar.gz --disable-music-mod-mikmod --enable-music-mod-modplug

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
EOF

chmod +x build-offline.sh

# Create README
cat > README.md <<'EOF'
# MechMania IV Vendored Dependencies

This directory contains all external dependencies needed to build MechMania IV,
preserved for long-term availability.

## Contents

- `source-packages/` - Source code for all dependencies
  - SDL2, SDL2_image, SDL2_ttf, libmodplug, SDL2_mixer libraries
  - CMake build system
- `docker-images/` - Docker base images (if Docker was available)
- `build-offline.sh` - Script to build from vendored sources

## Offline Building

To build MechMania IV without internet access:

1. Run `./build-offline.sh` (installs SDL dependencies into `vendor/local` without root)
2. Export helper vars (or let CMake detect automatically):
   ```bash
   export PKG_CONFIG_PATH="$(pwd)/vendor/local/lib/pkgconfig:$PKG_CONFIG_PATH"
   export CMAKE_PREFIX_PATH="$(pwd)/vendor/local:$CMAKE_PREFIX_PATH"
   ```
3. Build MechMania IV normally with CMake (`cmake -S . -B build && cmake --build build`)

## Docker Offline Build

Load the saved Docker images:
```bash
docker load < docker-images/alpine-3.18.tar.gz
docker load < docker-images/ubuntu-22.04.tar.gz
```

Then build using the Dockerfiles with `--network none` to ensure offline build.

## Verification

All packages include SHA256 checksums in `source-packages/SHA256SUMS`.
Verify with: `cd source-packages && sha256sum -c SHA256SUMS`

## Long-term Preservation

These vendored dependencies ensure MechMania IV can be built even if:
- GitHub disappears
- SDL.org goes offline
- Package repositories are discontinued
- Docker Hub shuts down

Last updated: $(date)
EOF

echo -e "\n==========================================="
echo "Dependency preservation complete!"
echo "Total size: $(du -sh . | cut -f1)"
echo ""
echo "These files ensure MechMania IV can be built"
echo "even without internet access in the future."
echo "==========================================="
