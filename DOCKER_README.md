# MechMania IV Docker Documentation

## Quick Start

### Easiest: Web Browser Interface
```bash
# Build and run the web version
docker build -f Dockerfile.web -t mechmania4-web .
docker run -p 6080:6080 mechmania4-web

# Open in browser
http://localhost:6080/vnc.html
```

No X11 configuration needed - works on any OS!

### Using the Launcher Script

#### Linux/macOS:
```bash
chmod +x mechmania4.sh
./mechmania4.sh web     # Web interface
./mechmania4.sh quick   # X11 graphics (Linux/Mac)
./mechmania4.sh test    # Headless test
```

#### Windows:
```cmd
mechmania4.bat
REM Then choose option 1 for web interface
```

## Available Docker Images

### 1. Standard Ubuntu Build (`Dockerfile`)
- **Size**: ~350MB
- **Base**: Ubuntu 22.04
- **Use**: Development, debugging
- **Features**: Full Ubuntu environment

```bash
docker build -t mechmania4 .
docker run -it mechmania4
```

### 2. Minimal Alpine Build (`Dockerfile.alpine`)
- **Size**: ~50MB
- **Base**: Alpine Linux 3.18
- **Use**: Production, CI/CD
- **Features**: Smallest footprint

```bash
docker build -f Dockerfile.alpine -t mechmania4-alpine .
docker run mechmania4-alpine ./quick_test.sh
```

### 3. Web Interface Build (`Dockerfile.web`)
- **Size**: ~450MB
- **Base**: Ubuntu 22.04 + VNC + noVNC
- **Use**: Easy access, demos, tournaments
- **Features**: Browser-based, no X11 needed

```bash
docker build -f Dockerfile.web -t mechmania4-web .
docker run -p 6080:6080 mechmania4-web
```

## Docker Compose Configurations

### Quick Game
```bash
docker-compose --profile quick up
```
Runs a complete match with X11 graphics.

### Tournament Mode
```bash
docker-compose --profile tournament up
```
- Separate containers for server, observer, teams
- Web-based observer on port 6080
- Automatic team connections

### Development Mode
```bash
docker-compose --profile dev up
```
- Volume mounts for live code changes
- Separate server and observer containers

### Headless Testing
```bash
docker-compose --profile test up
```
Runs automated tests without graphics.

## Display Options

### 1. X11 Forwarding (Linux)
```bash
docker run -it --rm \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  mechmania4 ./run_game.sh
```

### 2. X11 Forwarding (macOS)
Requires XQuartz installed:
```bash
# Start XQuartz first
open -a XQuartz

# In XQuartz preferences, enable "Allow connections from network clients"

# Run with display forwarding
docker run -it --rm \
  -e DISPLAY=host.docker.internal:0 \
  mechmania4 ./run_game.sh
```

### 3. Web Browser (Any OS)
```bash
docker run -p 6080:6080 mechmania4-web
```
Then open http://localhost:6080/vnc.html

### 4. Headless (No Display)
```bash
docker run mechmania4-alpine ./quick_test.sh
```

## Network Modes

### Host Networking (Linux only)
```bash
docker run --network host mechmania4 ./mm4serv
```

### Bridge Networking (Default)
```bash
# Server
docker run -p 2323:2323 mechmania4 ./mm4serv -h0.0.0.0

# Teams connect to host IP
docker run mechmania4 ./mm4team -h host.docker.internal
```

### Custom Network
```bash
# Create network
docker network create mechmania

# Run containers
docker run --network mechmania --name server mechmania4 ./mm4serv
docker run --network mechmania mechmania4 ./mm4team -hserver
```

## Persistence and Volumes

### Save Game Logs
```bash
docker run -v $(pwd)/logs:/logs mechmania4 \
  bash -c "./run_game.sh 2>&1 | tee /logs/game.log"
```

### Custom Team Code
```bash
docker run -v $(pwd)/myteam:/team mechmania4 \
  bash -c "cd /team && make && ./mm4team"
```

## Building for Different Architectures

### Multi-platform Build
```bash
docker buildx create --use
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -t mechmania4:multi \
  --push .
```

## Troubleshooting

### Container won't start
```bash
# Check logs
docker logs <container_id>

# Interactive debug
docker run -it --entrypoint /bin/bash mechmania4
```

### Graphics not working
```bash
# Test X11
docker run -it --rm \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  mechmania4 \
  bash -c "apt-get update && apt-get install -y x11-apps && xclock"
```

### Network issues
```bash
# List networks
docker network ls

# Inspect network
docker network inspect bridge

# Test connectivity
docker run mechmania4 nc -zv host.docker.internal 2323
```

## CI/CD Integration

### GitHub Actions
The included `.github/workflows/docker-build.yml` automatically:
- Builds all three Docker images on push
- Publishes to GitHub Container Registry
- Creates releases with pre-built images
- Runs automated tests

### Using Pre-built Images
```bash
# From GitHub Container Registry
docker pull ghcr.io/digitalmacgyver/mechmania4:latest
docker pull ghcr.io/digitalmacgyver/mechmania4:web
docker pull ghcr.io/digitalmacgyver/mechmania4:alpine
```

## Long-term Preservation

### Create Preservation Archive
```bash
chmod +x create-preservation-archive.sh
./create-preservation-archive.sh 1.0.0
```

Creates a complete archive with:
- Source code
- Docker images
- Compiled binaries
- Vendored dependencies
- Restoration instructions

### Download Dependencies for Offline Build
```bash
chmod +x vendor/download-dependencies.sh
./vendor/download-dependencies.sh
```

Downloads all dependencies locally for future offline builds.

## Security Considerations

### Run as Non-root User
All images create and use a `mechmania` user:
```dockerfile
RUN useradd -m mechmania
USER mechmania
```

### Resource Limits
```bash
docker run \
  --memory="512m" \
  --cpus="1" \
  mechmania4 ./quick_test.sh
```

### Read-only Filesystem
```bash
docker run --read-only \
  --tmpfs /tmp \
  mechmania4-alpine ./quick_test.sh
```

## Performance Optimization

### Build Cache
```bash
# Use BuildKit for better caching
DOCKER_BUILDKIT=1 docker build -t mechmania4 .

# Or with docker-compose
COMPOSE_DOCKER_CLI_BUILD=1 docker-compose build
```

### Layer Optimization
The Dockerfiles use multi-stage builds to minimize final image size:
- Build stage: Includes compilers and dev tools
- Runtime stage: Only includes necessary libraries

### Minimal Images
Use Alpine-based image for smallest footprint:
```bash
docker images mechmania4*
# mechmania4-alpine   ~50MB
# mechmania4          ~350MB
# mechmania4-web      ~450MB
```

## Advanced Usage

### Custom Entry Point
```bash
docker run --entrypoint /bin/bash mechmania4 -c "
  echo 'Starting custom game...'
  ./mm4serv -p2323 &
  sleep 2
  ./mm4team -p2323 &
  ./mm4team_vortex -p2323 &
  wait
"
```

### Environment Variables
```bash
docker run \
  -e MODE=tournament \
  -e PORT=3000 \
  -e HOST=0.0.0.0 \
  mechmania4-web
```

### Health Checks
```yaml
healthcheck:
  test: ["CMD", "nc", "-z", "localhost", "2323"]
  interval: 30s
  timeout: 10s
  retries: 3
```

## FAQ

**Q: Which Docker image should I use?**
- Web interface: Use `mechmania4-web` for easiest setup
- Development: Use standard `mechmania4` for full Ubuntu environment
- Production: Use `mechmania4-alpine` for minimal footprint

**Q: How do I connect teams from different computers?**
- Run server with `-h0.0.0.0` to listen on all interfaces
- Teams connect using server's IP address
- Ensure port 2323 is open in firewall

**Q: Can I run without Docker?**
- Yes, see main README.md for native build instructions
- Docker provides consistency across different systems

**Q: How do I save/load games?**
- Currently no save/load feature
- Use Docker volumes to preserve logs
- Record games with observer for replay

## Support

- GitHub Issues: https://github.com/digitalmacgyver/mechmania4/issues
- Documentation: See README.md, TEAM_API.md, CONTEST_RULES.md

---

*MechMania IV: The Vinyl Frontier - Dockerized for the future!*