#!/bin/bash
# Create a complete preservation archive for MechMania IV
# This ensures the game can be rebuilt and run decades from now

set -e

VERSION=${1:-"1.0.0"}
ARCHIVE_NAME="mechmania4-preservation-${VERSION}"
ARCHIVE_DIR="/tmp/${ARCHIVE_NAME}"

echo "Creating MechMania IV Preservation Archive v${VERSION}"
echo "===================================================="

# Clean and create archive directory
rm -rf "$ARCHIVE_DIR"
mkdir -p "$ARCHIVE_DIR"

echo -e "\n[1/6] Copying source code..."
cp -r . "$ARCHIVE_DIR/source"
cd "$ARCHIVE_DIR/source"
rm -rf .git build vendor/source-packages vendor/docker-images
cd -

echo -e "\n[2/6] Building Docker images..."
docker build -f Dockerfile -t mechmania4:preservation .
docker build -f Dockerfile.alpine -t mechmania4-alpine:preservation .
docker build -f Dockerfile.web -t mechmania4-web:preservation .

echo -e "\n[3/6] Saving Docker images..."
mkdir -p "$ARCHIVE_DIR/docker-images"
docker save mechmania4:preservation | gzip > "$ARCHIVE_DIR/docker-images/mechmania4.tar.gz"
docker save mechmania4-alpine:preservation | gzip > "$ARCHIVE_DIR/docker-images/mechmania4-alpine.tar.gz"
docker save mechmania4-web:preservation | gzip > "$ARCHIVE_DIR/docker-images/mechmania4-web.tar.gz"

echo -e "\n[4/6] Building static binaries..."
mkdir -p "$ARCHIVE_DIR/binaries/linux-x64"
docker run --rm -v "$ARCHIVE_DIR/binaries/linux-x64:/output" mechmania4-alpine:preservation sh -c "
    cp /home/mechmania/game/mm4* /output/
    cp -r /home/mechmania/game/gfx /output/
    cp /home/mechmania/game/graphics.reg /output/
    cp /home/mechmania/game/*.sh /output/
    chmod +x /output/*.sh /output/mm4*
"

echo -e "\n[5/6] Downloading dependencies..."
if [ -f vendor/download-dependencies.sh ]; then
    chmod +x vendor/download-dependencies.sh
    cd vendor
    ./download-dependencies.sh
    cd ..
    cp -r vendor "$ARCHIVE_DIR/vendor"
fi

echo -e "\n[6/6] Creating documentation..."
cat > "$ARCHIVE_DIR/RESTORATION_GUIDE.md" <<'EOF'
# MechMania IV Preservation Archive - Restoration Guide

This archive contains everything needed to restore and run MechMania IV,
even decades from now when original sources may no longer exist.

## Archive Contents

```
mechmania4-preservation/
├── source/                 # Complete source code
├── docker-images/          # Pre-built Docker images
├── binaries/              # Pre-compiled binaries
│   └── linux-x64/         # Linux x64 executables
├── vendor/                # All dependencies
│   ├── source-packages/   # SDL2, CMake source code
│   └── docker-images/     # Base OS images
└── RESTORATION_GUIDE.md   # This file
```

## Method 1: Use Pre-built Docker Images (Easiest)

```bash
# Load the Docker images
docker load < docker-images/mechmania4.tar.gz
docker load < docker-images/mechmania4-web.tar.gz

# Run the web version
docker run -p 6080:6080 mechmania4-web:preservation
# Open browser to http://localhost:6080/vnc.html
```

## Method 2: Use Pre-compiled Binaries

```bash
cd binaries/linux-x64
./run_game.sh

# Or run components separately:
./mm4serv -p2323         # Server
./mm4obs -p2323           # Observer
./mm4team -p2323          # Team AI
```

Requirements: Linux x64, SDL2 libraries

## Method 3: Build from Source (Most Reliable Long-term)

### With Internet:
```bash
cd source
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Without Internet (using vendored dependencies):
```bash
cd vendor
./build-offline.sh        # Install vendored dependencies
cd ../source
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Method 4: Manual Build (If all else fails)

1. Install a C++ compiler (GCC 7+ or Clang 5+)
2. Install SDL2 development libraries (or build from vendor/source-packages)
3. Compile each component:
   ```bash
   cd source/team/src
   g++ -o mm4serv mm4serv.C Server.C ServerNet.C World.C ... -lSDL2
   g++ -o mm4obs mm4obs_sdl.C ObserverSDL.C ... -lSDL2 -lSDL2_image -lSDL2_ttf
   ```

## Running the Game

### Quick Test
```bash
./quick_test.sh           # Runs headless match
```

### Full Game with Graphics
```bash
# Terminal 1: Server
./mm4serv -p2323

# Terminal 2: Observer
./mm4obs -p2323 -G

# Terminal 3: Team 1
./mm4team -p2323

# Terminal 4: Team 2
./mm4team_vortex -p2323
```

## Historical Context

MechMania IV: The Vinyl Frontier was created for the 1998 ACM programming
competition at the University of Illinois. This preservation archive was
created in 2024 to ensure this piece of computing history remains playable.

## Troubleshooting

### Docker won't load images
- Try an older Docker version
- Extract and import layers manually

### Binaries won't run
- Check architecture: `file mm4serv`
- Install SDL2: `apt-get install libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-ttf-2.0-0`

### Build fails
- Use vendored dependencies in vendor/source-packages
- Try older compiler flags: `-std=c++98`

## Preservation Notes

This archive follows digital preservation best practices:
- Multiple restoration paths (Docker, binary, source)
- Vendored dependencies (no external downloads required)
- Complete documentation
- Historical context preserved

Created: $(date)
Version: ${VERSION}
EOF

# Create main archive README
cat > "$ARCHIVE_DIR/README.md" <<EOF
# MechMania IV: The Vinyl Frontier - Preservation Archive

## Quick Start

### Option 1: Web Browser (Easiest)
\`\`\`bash
docker load < docker-images/mechmania4-web.tar.gz
docker run -p 6080:6080 mechmania4-web:preservation
# Open http://localhost:6080/vnc.html
\`\`\`

### Option 2: Pre-built Binaries
\`\`\`bash
cd binaries/linux-x64
./run_game.sh
\`\`\`

### Option 3: Build from Source
\`\`\`bash
cd source && mkdir build && cd build
cmake .. && make
./run_game.sh
\`\`\`

## Contents
- Complete source code
- Pre-built Docker images
- Compiled binaries
- All dependencies vendored
- Full documentation

See RESTORATION_GUIDE.md for detailed instructions.

Version: ${VERSION}
Created: $(date)
EOF

# Create final archive
echo -e "\nCreating final archive..."
cd /tmp
tar -czf "${ARCHIVE_NAME}.tar.gz" "${ARCHIVE_NAME}"

# Calculate checksums
sha256sum "${ARCHIVE_NAME}.tar.gz" > "${ARCHIVE_NAME}.tar.gz.sha256"

# Move to original directory
mv "${ARCHIVE_NAME}.tar.gz" "${ARCHIVE_NAME}.tar.gz.sha256" "$(cd - >/dev/null && pwd)/"

echo -e "\n===================================================="
echo "Preservation archive created successfully!"
echo "Files created:"
echo "  - ${ARCHIVE_NAME}.tar.gz"
echo "  - ${ARCHIVE_NAME}.tar.gz.sha256"
echo ""
echo "Archive size: $(du -h ${ARCHIVE_NAME}.tar.gz | cut -f1)"
echo ""
echo "This archive contains everything needed to run"
echo "MechMania IV even decades from now!"
echo "===================================================="