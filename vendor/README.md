# MechMania IV Vendored Dependencies

This directory contains all external dependencies needed to build MechMania IV,
preserved for long-term availability.

## Contents

- `source-packages/` - Source code for all dependencies
  - SDL2, SDL2_image, SDL2_ttf, SDL2_mixer libraries
  - CMake build system
- `docker-images/` - Docker base images (if Docker was available)
- `build-offline.sh` - Script to build from vendored sources

## Offline Building

To build MechMania IV without internet access:

1. Run `./build-offline.sh` to install dependencies (CMake will also auto-run this when needed unless `MM4_AUTO_VENDOR_DEPS=OFF`)
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
