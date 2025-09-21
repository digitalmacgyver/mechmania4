# MechMania IV - Docker Installation & Play Guide

This guide provides step-by-step instructions for installing and running MechMania IV using Docker on Windows, macOS, and Linux.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Quick Start (All Platforms)](#quick-start-all-platforms)
- [Windows Instructions](#windows-instructions)
- [macOS Instructions](#macos-instructions)
- [Linux Instructions](#linux-instructions)
- [Running Different Game Modes](#running-different-game-modes)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Install Docker
- **Windows**: [Docker Desktop for Windows](https://docs.docker.com/desktop/windows/install/)
- **macOS**: [Docker Desktop for Mac](https://docs.docker.com/desktop/mac/install/)
- **Linux**: [Docker Engine](https://docs.docker.com/engine/install/)

### Download MechMania IV
```bash
git clone https://github.com/digitalmacgyver/mechmania4.git
cd mechmania4
```

Or download as ZIP from: https://github.com/digitalmacgyver/mechmania4/archive/refs/heads/master.zip

---

## Quick Start (All Platforms)

The **easiest method** works identically on all platforms - use the web browser interface:

### Step 1: Build the Web Docker Image
Open a terminal/command prompt in the mechmania4 directory:
```bash
docker build -f Dockerfile.web -t mechmania4-web .
```
**⚠️ IMPORTANT: This build step is required! It will take 5-10 minutes the first time.**
**The images must be built locally - they are not available from Docker Hub.**

### Step 2: Run the Game
```bash
docker run -d -p 6080:6080 --name mm4-web mechmania4-web
```

### Step 3: Open in Browser
Open your web browser to: **http://localhost:6080/vnc.html**

You'll see the MechMania IV game running in your browser!

### Step 4: Stop the Game
```bash
docker stop mm4-web
docker rm mm4-web
```

---

## Windows Instructions

### Method 1: Using the Batch Script (Recommended)

1. **Open Command Prompt or PowerShell**
   - Press `Win+R`, type `cmd`, press Enter

2. **Navigate to mechmania4 directory**
   ```cmd
   cd C:\path\to\mechmania4
   ```

3. **Run the launcher**
   ```cmd
   mechmania4.bat
   ```

4. **Choose from the menu:**
   - Press `1` for Web Browser Game (recommended)
   - Press `2` for Headless Test
   - Press `4` to Build Docker Images (first time only)

### Method 2: Manual Docker Commands

1. **Build the image:**
   ```powershell
   docker build -f Dockerfile.web -t mechmania4-web .
   ```

2. **Run the game:**
   ```powershell
   docker run -d -p 6080:6080 --name mm4 mechmania4-web
   ```

3. **Open browser to:** http://localhost:6080/vnc.html

4. **When done:**
   ```powershell
   docker stop mm4
   docker rm mm4
   ```

### Running a Tournament (Windows)

```powershell
# Build all images first
docker build -f Dockerfile -t mechmania4 .
docker build -f Dockerfile.alpine -t mechmania4-alpine .
docker build -f Dockerfile.web -t mechmania4-web .

# Run tournament
docker-compose --profile tournament up
```
Open browser to: http://localhost:6080/vnc.html

---

## macOS Instructions

### Method 1: Using the Shell Script (Recommended)

1. **Open Terminal**
   - Press `Cmd+Space`, type "Terminal", press Enter

2. **Navigate to mechmania4 directory**
   ```bash
   cd ~/Downloads/mechmania4
   ```

3. **Make script executable (first time only)**
   ```bash
   chmod +x mechmania4.sh
   ```

4. **Run the launcher**
   ```bash
   ./mechmania4.sh
   ```

5. **Choose from menu:**
   - `2` - Web Browser Game (recommended for macOS)
   - `1` - X11 Graphics (requires XQuartz - see below)
   - `4` - Headless Test

### Method 2: With XQuartz (Native Window)

1. **Install XQuartz** (if you want native window instead of browser)
   - Download from: https://www.xquartz.org/
   - Install and restart your Mac

2. **Configure XQuartz:**
   - Open XQuartz
   - Go to XQuartz → Preferences → Security
   - Check "Allow connections from network clients"
   - Restart XQuartz

3. **Run with X11:**
   ```bash
   docker run -it --rm \
     -e DISPLAY=host.docker.internal:0 \
     mechmania4 \
     ./run_game.sh
   ```

### Running a Tournament (macOS)

```bash
# Build all images first
docker build -f Dockerfile -t mechmania4 .
docker build -f Dockerfile.alpine -t mechmania4-alpine .
docker build -f Dockerfile.web -t mechmania4-web .

# Run tournament
docker-compose --profile tournament up
```
Open browser to: http://localhost:6080/vnc.html

---

## Linux Instructions

### Method 1: Using the Shell Script (Recommended)

1. **Open Terminal**

2. **Navigate to mechmania4 directory**
   ```bash
   cd ~/mechmania4
   ```

3. **Make script executable (first time only)**
   ```bash
   chmod +x mechmania4.sh
   ```

4. **Run the launcher**
   ```bash
   ./mechmania4.sh
   ```

5. **Choose from menu:**
   - `1` - Quick Game with X11 Graphics (native window)
   - `2` - Web Browser Game
   - `3` - Tournament Mode
   - `4` - Headless Test

### Method 2: Native X11 Window (Linux Exclusive Feature)

Linux users can run the game with native X11 graphics:

```bash
# Allow X11 connections
xhost +local:docker

# Run with X11 forwarding
docker run -it --rm \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  mechmania4 \
  ./run_game.sh
```

### Running a Tournament (Linux)

```bash
# Using docker-compose
docker-compose --profile tournament up

# Or manually:
./mechmania4.sh
# Then select option 3
```

---

## Running Different Game Modes

### 1. Quick Match: ChromeFunk vs Vortex

**All Platforms:**
```bash
docker run -d -p 6080:6080 mechmania4-web
```
Open: http://localhost:6080/vnc.html

### 2. Custom Teams Match

**Create custom game with specific teams:**
```bash
# Start server
docker run -d --name server -p 2323:2323 \
  mechmania4-alpine ./mm4serv -p2323 -h0.0.0.0

# Start observer (web)
docker run -d --name observer -p 6080:6080 \
  -e MODE=observer -e HOST=host.docker.internal \
  mechmania4-web

# Start Team 1 (Groogroo)
docker run -d --name team1 \
  mechmania4-alpine \
  ./mm4team_groogroo -p2323 -hhost.docker.internal -n"Groogroo"

# Start Team 2 (Vortex)
docker run -d --name team2 \
  mechmania4-alpine \
  ./mm4team_vortex -p2323 -hhost.docker.internal -n"Team Vortex"
```

Watch at: http://localhost:6080/vnc.html

**Cleanup:**
```bash
docker stop server observer team1 team2
docker rm server observer team1 team2
```

### 3. Headless Testing (No Graphics)

**Quick test to verify everything works:**
```bash
docker run --rm mechmania4-alpine ./quick_test.sh
```

You'll see text output of the match results.

### 4. Development Mode (Edit and Test)

**For developers who want to modify team code:**

1. Create your team file: `myteam.C`

2. Run with mounted directory:
```bash
docker run -it --rm \
  -v $(pwd):/workspace \
  mechmania4 \
  bash -c "cd /workspace && g++ -o myteam myteam.C -lmm4 && ./myteam"
```

---

## Team Selection Options

### Available Teams

1. **Chrome Funkadelic** (Default)
   - Basic reactive AI
   - File: `mm4team`

2. **Groogroo** (Advanced)
   - Predictive targeting
   - Smart resource management
   - File: `mm4team_groogroo`

3. **Team Vortex** (Competitive)
   - Zone-based strategy
   - Optimized for tournaments
   - File: `mm4team_vortex`

### Running Specific Matchups

**Groogroo vs Vortex:**
```bash
docker run -p 6080:6080 -e TEAMS="groogroo vortex" mechmania4-web
```

**All three teams (requires custom setup):**
```bash
# MechMania only supports 2 teams per match
# Run multiple matches for tournament
```

---

## Troubleshooting

### Docker Issues

**"Cannot connect to Docker daemon"**
- **Windows/Mac**: Ensure Docker Desktop is running (check system tray)
- **Linux**: Start Docker service: `sudo systemctl start docker`

**"Port already in use"**
```bash
# Find what's using port 6080
# Windows:
netstat -ano | findstr :6080

# Mac/Linux:
lsof -i :6080

# Use different port:
docker run -p 7080:6080 mechmania4-web
# Then open: http://localhost:7080/vnc.html
```

### Display Issues

**Black screen in browser:**
- Wait 10-15 seconds for services to start
- Refresh the browser page
- Check Docker logs: `docker logs mm4-web`

**"Connection refused" in browser:**
- Ensure container is running: `docker ps`
- Check correct URL: http://localhost:6080/vnc.html
- Try: http://127.0.0.1:6080/vnc.html

**Poor performance/lag:**
- Close other browser tabs
- Use Chrome or Firefox (best compatibility)
- Try reducing game ships: `-n3` flag

### Game Issues

**Game ends immediately:**
- Normal behavior for quick_test.sh
- Matches last 5 minutes by default
- Check server logs: `docker logs <container>`

**Teams not connecting:**
- Ensure server started first
- Check network settings
- Use `host.docker.internal` for cross-container communication

### Platform-Specific Issues

**Windows: "docker" not recognized:**
- Restart Command Prompt after Docker Desktop install
- Add Docker to PATH manually if needed

**macOS: XQuartz not working:**
- Logout and login after XQuartz install
- In XQuartz settings, enable "Allow connections from network clients"
- Try: `xhost +` before running Docker

**Linux: X11 permission denied:**
```bash
xhost +local:docker
# Or more secure:
xhost +local:$(docker inspect -f '{{ .Config.Hostname }}' <container>)
```

---

## Getting Help

1. **Check the documentation:**
   - `DOCKER_README.md` - Detailed Docker information
   - `TEAM_API.md` - Programming your own team
   - `CONTEST_RULES.md` - Game rules and mechanics

2. **View logs:**
   ```bash
   docker logs <container-name>
   ```

3. **Interactive debugging:**
   ```bash
   docker run -it --entrypoint /bin/bash mechmania4
   ```

4. **Report issues:**
   https://github.com/digitalmacgyver/mechmania4/issues

---

## Quick Reference Card

```bash
# Build images (first time)
docker build -f Dockerfile.web -t mechmania4-web .

# Run web version (easiest)
docker run -d -p 6080:6080 --name mm4 mechmania4-web
# Open: http://localhost:6080/vnc.html

# Stop game
docker stop mm4 && docker rm mm4

# Run headless test
docker run --rm mechmania4-alpine ./quick_test.sh

# View running containers
docker ps

# Clean up everything
docker-compose down
docker system prune
```

---

*Enjoy playing MechMania IV: The Vinyl Frontier!*