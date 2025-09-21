# Docker Development & Archival Strategy

## Overview

This document explains the Docker-based preservation and distribution strategy for MechMania IV, ensuring the game remains playable for decades to come.

## Archival Philosophy

Our approach follows the "belt and suspenders" principle - multiple redundant methods to ensure long-term viability:

1. **Source Code Preservation** - Complete buildable source
2. **Binary Preservation** - Pre-compiled executables
3. **Container Preservation** - Ready-to-run Docker images
4. **Dependency Vendoring** - All external dependencies stored locally
5. **Documentation Redundancy** - Multiple guides for different scenarios

## File Structure & Purpose

### Core Docker Files

#### `Dockerfile` (Standard Build)
- **Purpose**: Primary development and debugging environment
- **Base**: Ubuntu 22.04 LTS (supported until 2032)
- **Size**: ~350MB
- **Use Case**: Developers who need full Linux tooling
- **Why Ubuntu**: Maximum compatibility, extensive documentation

#### `Dockerfile.alpine` (Minimal Build)
- **Purpose**: Smallest possible production container
- **Base**: Alpine Linux 3.18 (musl libc)
- **Size**: ~50MB
- **Use Case**: CI/CD pipelines, resource-constrained environments
- **Why Alpine**: Minimal attack surface, long-term stability

#### `Dockerfile.web` (Browser Interface)
- **Purpose**: Zero-configuration browser-based access
- **Base**: Ubuntu + X11 + VNC + noVNC
- **Size**: ~450MB
- **Use Case**: Demos, tournaments, cross-platform access
- **Why This Approach**: Eliminates X11 forwarding complexity

#### `docker-compose.yml`
- **Purpose**: Orchestrated multi-container deployments
- **Profiles**:
  - `quick`: Single-container quick game
  - `tournament`: Multi-container competitive setup
  - `web`: Browser-based tournament viewer
  - `dev`: Development with live code mounting
  - `test`: Automated testing
- **Why Profiles**: Different use cases without multiple files

### Launcher Scripts

#### `mechmania4.sh` (Linux/macOS)
- **Purpose**: User-friendly interactive launcher
- **Features**:
  - Auto-detects platform (Linux vs macOS/XQuartz)
  - Interactive menu system
  - Handles X11 forwarding complexity
  - Command-line shortcuts for power users

#### `mechmania4.bat` (Windows)
- **Purpose**: Windows-native launcher
- **Features**:
  - Works with Docker Desktop
  - Focuses on web interface (best for Windows)
  - Simple menu system

### Preservation Infrastructure

#### `vendor/download-dependencies.sh`
- **Purpose**: Download all external dependencies for offline builds
- **Downloads**:
  - SDL2 source tarballs (2.28.5, 2.8.2, 2.22.0)
  - CMake source (3.27.9)
  - Docker base images (Alpine 3.18, Ubuntu 22.04)
- **Creates**: SHA256 checksums for verification
- **Why**: External sources may disappear

#### `create-preservation-archive.sh`
- **Purpose**: Create complete self-contained archive
- **Includes**:
  1. Full source code
  2. Pre-built Docker images
  3. Compiled Linux binaries
  4. Vendored dependencies
  5. Restoration documentation
- **Output**: Single `.tar.gz` with everything needed
- **Why**: "Time capsule" approach for decades-long preservation

### CI/CD Integration

#### `.github/workflows/docker-build.yml`
- **Purpose**: Automated building and publishing
- **Actions**:
  - Builds all three Docker variants
  - Pushes to GitHub Container Registry
  - Creates releases with binaries
  - Runs automated tests
- **Triggers**: Push to master, tags, pull requests
- **Why**: Ensures working builds always available

## Design Decisions

### Multi-Stage Builds
All Dockerfiles use multi-stage builds:
```dockerfile
FROM base AS builder  # Build stage with compilers
FROM base            # Runtime with only libraries
```
**Why**: Reduces image size by ~70%, improves security

### Non-Root Users
All containers run as unprivileged user:
```dockerfile
RUN useradd -m mechmania
USER mechmania
```
**Why**: Security best practice, prevents container escape

### Supervisor for Web Version
Web container uses supervisord to manage multiple services:
- Xvfb (virtual framebuffer)
- x11vnc (VNC server)
- novnc (web proxy)
- fluxbox (window manager)

**Why**: Single container simplicity vs docker-compose complexity

### Vendored Dependencies
We vendor source code, not binaries:
- SDL2 source tarballs
- Build from source in container

**Why**: Source code more portable across architectures

## Preservation Layers

### Layer 1: GitHub
- Source code in git
- Docker images in GitHub Container Registry
- Releases with binaries

### Layer 2: Docker Hub (Future)
- Public images for easy access
- Automated builds from GitHub

### Layer 3: Local Archives
- `create-preservation-archive.sh` output
- Can be stored on physical media
- Complete offline restoration

### Layer 4: Vendored Dependencies
- All external dependencies downloaded
- Offline build capability
- Protected against upstream changes

## Testing Strategy

### Build Testing
```bash
# Test all builds work
docker build -f Dockerfile -t test1 .
docker build -f Dockerfile.alpine -t test2 .
docker build -f Dockerfile.web -t test3 .
```

### Runtime Testing
```bash
# Headless test
docker run --rm test2 ./quick_test.sh

# Interactive test
docker run --rm -it test1 ./run_game.sh
```

### Preservation Testing
```bash
# Create archive
./create-preservation-archive.sh

# Test restoration from archive
tar -xzf mechmania4-preservation-*.tar.gz
cd mechmania4-preservation-*/
# Follow RESTORATION_GUIDE.md
```

## Future-Proofing Considerations

### 2030 Scenario
- Ubuntu 22.04 still supported (until 2032)
- Docker likely still dominant
- GitHub/Microsoft likely still operating
- **Action**: Update to Ubuntu 24.04 LTS

### 2040 Scenario
- Docker may be legacy
- GitHub status unknown
- SDL2 may be obsolete
- **Mitigation**: Preservation archive on physical media

### 2050+ Scenario
- Complete technology shift likely
- Emulation may be required
- **Mitigation**:
  - Source code ensures recompilation possible
  - Documentation explains game mechanics
  - Multiple restoration paths in archive

## Maintenance Guidelines

### Annual Review
1. Test all Docker builds still work
2. Update base images if needed
3. Refresh vendored dependencies
4. Create new preservation archive
5. Test restoration procedures

### When to Update Base Images
- Security vulnerabilities in base
- Base image approaching EOL
- Better alternative available
- Major SDL2 version update

### Version Tagging Strategy
```bash
git tag -a v1.0.0 -m "Initial Docker release"
git tag -a v1.0.1 -m "Security updates"
git tag -a v1.1.0 -m "New features"
```

Tags trigger GitHub Actions to create releases with:
- Source code archives
- Docker images
- Compiled binaries
- Preservation archive

## Troubleshooting Development

### Docker Build Failures
```bash
# Clean rebuild
docker build --no-cache -f Dockerfile .

# Debug build
docker build --progress=plain -f Dockerfile .

# Interactive debug
docker run -it --entrypoint /bin/bash <image>
```

### Size Optimization
```bash
# Analyze layers
docker history <image>

# Find large files
docker run --rm <image> du -h / | sort -h | tail -20
```

### Network Issues
```bash
# Test container networking
docker run --rm alpine ping -c 1 google.com

# Check Docker networks
docker network ls
docker network inspect bridge
```

## Contributing

### Adding New Docker Variant
1. Create `Dockerfile.newvariant`
2. Add to `docker-compose.yml`
3. Update `.github/workflows/docker-build.yml`
4. Document in `DOCKER_README.md`
5. Test with `create-preservation-archive.sh`

### Updating Dependencies
1. Edit version numbers in `vendor/download-dependencies.sh`
2. Run script to download new versions
3. Test builds with new dependencies
4. Update documentation

## License Considerations

All vendored dependencies are open source:
- SDL2: zlib license (very permissive)
- Alpine Linux: Various open source
- Ubuntu: Various open source
- CMake: BSD 3-clause

This ensures legal distribution of preservation archive.

---

*This development documentation ensures future maintainers understand the preservation strategy and can continue the work of keeping MechMania IV playable for decades to come.*