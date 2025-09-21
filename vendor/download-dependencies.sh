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

if [ ! -f "source-packages/SDL2-${SDL2_VERSION}.tar.gz" ]; then
    wget -P source-packages/ "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz"
fi

if [ ! -f "source-packages/SDL2_image-${SDL2_IMAGE_VERSION}.tar.gz" ]; then
    wget -P source-packages/ "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image-${SDL2_IMAGE_VERSION}.tar.gz"
fi

if [ ! -f "source-packages/SDL2_ttf-${SDL2_TTF_VERSION}.tar.gz" ]; then
    wget -P source-packages/ "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf-${SDL2_TTF_VERSION}.tar.gz"
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

VENDOR_DIR="$(dirname "$0")"
BUILD_DIR="/tmp/mechmania-build"

echo "Building MechMania IV from vendored dependencies..."
echo "No internet connection required!"

# Extract and build SDL2
mkdir -p $BUILD_DIR
cd $BUILD_DIR

echo "Building SDL2..."
tar -xzf $VENDOR_DIR/source-packages/SDL2-*.tar.gz
cd SDL2-*
./configure --prefix=/usr/local
make -j$(nproc)
sudo make install
cd ..

echo "Building SDL2_image..."
tar -xzf $VENDOR_DIR/source-packages/SDL2_image-*.tar.gz
cd SDL2_image-*
./configure --prefix=/usr/local
make -j$(nproc)
sudo make install
cd ..

echo "Building SDL2_ttf..."
tar -xzf $VENDOR_DIR/source-packages/SDL2_ttf-*.tar.gz
cd SDL2_ttf-*
./configure --prefix=/usr/local
make -j$(nproc)
sudo make install
cd ..

echo "Dependencies installed! Now build MechMania IV:"
echo "  cd /path/to/mechmania4"
echo "  mkdir build && cd build"
echo "  cmake .."
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
  - SDL2, SDL2_image, SDL2_ttf libraries
  - CMake build system
- `docker-images/` - Docker base images (if Docker was available)
- `build-offline.sh` - Script to build from vendored sources

## Offline Building

To build MechMania IV without internet access:

1. Run `./build-offline.sh` to install dependencies
2. Build MechMania IV normally with CMake

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