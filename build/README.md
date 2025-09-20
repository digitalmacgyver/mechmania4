# MechMania IV - Build Instructions

## Overview
MechMania IV is a space combat programming competition game from 1998, modernized for contemporary Linux systems. Teams program AI-controlled spaceships to collect resources and compete in a real-time strategy environment.

## Prerequisites

### Required Dependencies
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-ttf-dev \
    pkg-config

# Fedora/RHEL
sudo dnf install -y \
    gcc-c++ \
    cmake \
    SDL2-devel \
    SDL2_image-devel \
    SDL2_ttf-devel \
    pkg-config

# Arch Linux
sudo pacman -S \
    base-devel \
    cmake \
    sdl2 \
    sdl2_image \
    sdl2_ttf \
    pkg-config
```

### Optional Dependencies
- `gdb` - For debugging
- `valgrind` - For memory analysis
- `git` - For version control

## Building the Project

### Quick Build
```bash
# From the mm_claude directory
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### Build Options
```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build without graphics (headless server/teams only)
cmake -DBUILD_WITH_GRAPHICS=OFF ..

# Build with legacy X11 graphics instead of SDL2
cmake -DUSE_SDL2=OFF ..
```

## Executables

After building, you'll have the following executables:

### Core Components
- **`mm4serv`** - Game server that manages the world and physics
- **`mm4obs`** - Graphical observer client for viewing matches
- **`mm4team`** - Basic team AI client

### Team Implementations
- **`mm4team_groogroo`** - Advanced predictive AI team
- **`mm4team_vortex`** - Zone-based strategy team

## Running the Game

### Quick Start (Single Machine)
```bash
# Terminal 1: Start server
./mm4serv -p2323 -hlocalhost

# Terminal 2: Start first team
./mm4team -p2323 -hlocalhost

# Terminal 3: Start second team
./mm4team_vortex -p2323 -hlocalhost

# Terminal 4: Start observer (optional)
./mm4obs -p2323 -hlocalhost
```

### Using Helper Scripts
```bash
# Run a match with observer
./run_game.sh

# Run Groogroo team vs ChromeFunk
./run_game.sh groogroo

# Quick headless test
./quick_test.sh

# Run Vortex team matches
./run_vortex.sh
```

### Command Line Options

#### Server (`mm4serv`)
```
mm4serv [-pport] [-hhostname] [-nnumShips] [-ccredits]
  -p: Port number (default: 2323)
  -h: Hostname (default: localhost)
  -n: Ships per team (default: 4, max: 10)
  -c: Starting credits (default: 60)
```

#### Observer (`mm4obs`)
```
mm4obs [-R] [-G] [-pport] [-hhostname]
  -R: Reconnect mode (auto-reconnect after disconnect)
  -G: Full graphics mode with sprites
  -p: Port number (default: 2323)
  -h: Hostname (default: localhost)
```

#### Team Clients
```
mm4team [-pport] [-hhostname] [-nteamname]
  -p: Port number (default: 2323)
  -h: Hostname (default: localhost)
  -n: Team name
```

## Observer Controls

When running the observer (`mm4obs`):

| Key | Function |
|-----|----------|
| **G** | Toggle sprite mode (detailed graphics) |
| **N** | Toggle object names |
| **V** | Toggle velocity vectors |
| **Space** | Toggle logo display (useful between matches) |
| **ESC/Q** | Quit observer |

## File Structure

```
build/
├── mm4serv            # Server executable
├── mm4obs             # Observer executable
├── mm4team*           # Team executables
├── graphics.reg       # Sprite registry file
├── gfx/              # Sprite graphics (XPM files)
├── run_*.sh          # Helper scripts
└── README.md         # This file
```

## Network Ports

- Default port: **2323**
- Safe range for testing: **2323-2400**
- Multiple games can run simultaneously on different ports

## Graphics Modes

### Geometric Mode (Default)
- Simple shapes and lines
- Lower CPU usage
- Good for development/debugging

### Sprite Mode (Press G)
- Detailed XPM sprites
- Ship thrust animations
- Damage impact effects
- Professional appearance

## Troubleshooting

### Build Issues

**CMake can't find SDL2:**
```bash
# Ensure pkg-config can find SDL2
pkg-config --libs sdl2

# If not found, set PKG_CONFIG_PATH
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

**Missing graphics.reg or sprites:**
```bash
# Create symbolic links to graphics resources
ln -sf ../team/src/graphics.reg .
ln -sf ../team/src/gfx .
```

### Runtime Issues

**Observer shows "Expected 400 sprites, found 0":**
- Ensure graphics.reg and gfx/ directory are in the build directory
- Run the provided linking commands above

**"Connection refused" errors:**
- Ensure server is running before starting teams/observer
- Check firewall isn't blocking local connections
- Verify correct port number

**Observer freezes:**
- The observer now supports disconnected operation
- Press Space to show logo while waiting for server
- Use -R flag for auto-reconnect mode

### Performance

**Slow graphics:**
- Toggle sprite mode off with G key
- Use geometric rendering for better performance
- Consider running headless (no observer)

**High CPU usage:**
- Normal for real-time simulation
- Reduce ship count with -n flag on server
- Run without observer for testing

## Development

### Adding a New Team

1. Create team implementation in `teams/yourteam/`
2. Implement required interface methods
3. Add to CMakeLists.txt
4. Rebuild project

### Debugging

```bash
# Run with gdb
gdb ./mm4serv
(gdb) run -p2323

# Memory checking
valgrind --leak-check=full ./mm4serv -p2323

# Core dump analysis
gdb ./mm4serv core
```

## Competition Rules

See `CONTEST_RULES.md` for detailed game mechanics and scoring.

## API Documentation

See `TEAM_API.md` for team programming interface documentation.

## Credits

Original MechMania IV by ACM@UIUC (1998)
Modern port and SDL2 implementation (2024)

## License

Educational use - maintains compatibility with original MechMania IV competition.