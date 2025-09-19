# MechMania IV: The Vinyl Frontier - Modernized Edition

This is a modernized version of the 1998 MechMania IV programming contest code, updated to run on current Linux systems with SDL2 graphics.

## Changes from Original

### Phase 1: C++ Modernization
- Updated all deprecated C++ headers (iostream.h → iostream)
- Fixed type definitions (BOOL → bool)
- Added missing mathematical constants
- Fixed socket API compatibility (socklen_t)
- Migrated from custom Makefiles to CMake

### Phase 2: SDL2 Graphics Migration
- Replaced X11/Xlib with SDL2 for cross-platform graphics
- Created SDL2Graphics abstraction layer
- Implemented modern event handling
- Added TTF font support for text rendering
- Maintained backward compatibility with original game logic

## Building

### Prerequisites

```bash
# Install required packages (Ubuntu/Debian)
sudo apt install build-essential cmake
sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
```

### Compilation

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DBUILD_WITH_GRAPHICS=ON -DUSE_SDL2=ON

# Build all targets
make -j4
```

### Build Options

- `BUILD_WITH_GRAPHICS=ON/OFF` - Enable/disable graphics support (default: ON)
- `USE_SDL2=ON/OFF` - Use SDL2 instead of X11 for graphics (default: ON)

## Running the Game

### 1. Start the Server

```bash
./mm4serv [-pport] [-Tnumteams]
# Example: ./mm4serv -p2323 -T2
```

Options:
- `-p<port>` - Port number (default: 2323)
- `-T<teams>` - Number of teams (default: 2)

### 2. Connect Team Clients

```bash
./mm4team [-pport] [-hhostname]
# Example: ./mm4team -p2323 -hlocalhost
```

Options:
- `-p<port>` - Port number (default: 2323)
- `-h<hostname>` - Server hostname (default: localhost)

### 3. Start the Observer (Optional)

```bash
./mm4obs [-pport] [-hhostname] [-G] [-R]
# Example: ./mm4obs -p2323 -hlocalhost -G
```

Options:
- `-p<port>` - Port number (default: 2323)
- `-h<hostname>` - Server hostname (default: localhost)
- `-G` - Enable full graphics mode
- `-R` - Auto-reconnect on disconnect

## Observer Controls

When running the SDL2 observer:
- `ESC` or `Q` - Quit
- `N` - Toggle name display
- `V` - Toggle velocity vectors
- `SPACE` - Cycle through attractor modes

## Game Overview

MechMania IV: The Vinyl Frontier is a space combat and resource gathering game where teams compete to:
- Collect resources (Vinyl and Uranium asteroids)
- Attack enemy bases and ships
- Defend their own station
- Score points through strategic gameplay

The game runs for 300 seconds (5 minutes) per match.

## Architecture

- **mm4serv** - Game server managing physics simulation and game rules
- **mm4team** - Team client connecting to server and executing AI strategies
- **mm4obs** - Observer client for visualizing the game state

## Original Credits

MechMania IV was created in 1998 by Misha Voloshin and the University of Illinois ACM chapter for their annual programming contest.

## Modernization

Updated in 2024 to run on modern Linux systems with current C++ standards and SDL2 graphics.