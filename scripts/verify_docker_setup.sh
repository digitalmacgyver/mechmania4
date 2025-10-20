#!/bin/bash
# Docker Setup Verification Script
# Verifies that Docker infrastructure is up-to-date and functional

set -e

echo "========================================"
echo "MechMania IV Docker Verification"
echo "========================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if Docker is available
echo "1. Checking Docker availability..."
if ! command -v docker &> /dev/null; then
    echo -e "${RED}✗ Docker not found${NC}"
    echo "  Please install Docker first"
    exit 1
fi
echo -e "${GREEN}✓ Docker found${NC}"
echo ""

# Check Docker daemon
echo "2. Checking Docker daemon..."
if ! docker info &> /dev/null; then
    echo -e "${RED}✗ Docker daemon not running${NC}"
    echo "  Please start Docker daemon"
    exit 1
fi
echo -e "${GREEN}✓ Docker daemon running${NC}"
echo ""

# Check if source files are present
echo "3. Checking source files..."
MISSING_FILES=0

# Check Dockerfiles
for file in Dockerfile Dockerfile.alpine Dockerfile.web docker-compose.yml; do
    if [ ! -f "$file" ]; then
        echo -e "${RED}✗ Missing: $file${NC}"
        ((MISSING_FILES++))
    else
        echo -e "${GREEN}✓ Found: $file${NC}"
    fi
done

# Check if build directory exists with executables
if [ ! -d "build" ] || [ ! -f "build/mm4serv" ]; then
    echo -e "${RED}✗ Build directory missing or incomplete${NC}"
    echo "  Run: mkdir -p build && cd build && cmake .. && make"
    exit 1
fi
echo -e "${GREEN}✓ Build directory exists with executables${NC}"
echo ""

# Check if graphics resources exist
echo "4. Checking graphics resources..."
if [ ! -d "team/src/gfx" ]; then
    echo -e "${RED}✗ Missing: team/src/gfx${NC}"
    ((MISSING_FILES++))
else
    echo -e "${GREEN}✓ Found: team/src/gfx${NC}"
fi

if [ ! -f "team/src/graphics.reg" ]; then
    echo -e "${RED}✗ Missing: team/src/graphics.reg${NC}"
    ((MISSING_FILES++))
else
    echo -e "${GREEN}✓ Found: team/src/graphics.reg${NC}"
fi

if [ ! -d "team/src/fonts" ]; then
    echo -e "${RED}✗ Missing: team/src/fonts${NC}"
    ((MISSING_FILES++))
else
    echo -e "${GREEN}✓ Found: team/src/fonts${NC}"
fi
echo ""

if [ $MISSING_FILES -gt 0 ]; then
    echo -e "${RED}✗ $MISSING_FILES required files missing${NC}"
    exit 1
fi

# Check for team binaries
echo "5. Checking team binaries..."
TEAM_COUNT=0
for team in build/mm4team*; do
    if [ -f "$team" ]; then
        echo -e "${GREEN}✓ Found: $(basename $team)${NC}"
        ((TEAM_COUNT++))
    fi
done
echo "  Total teams found: $TEAM_COUNT"
echo ""

# List existing Docker images
echo "6. Checking existing Docker images..."
if docker images | grep -q "mechmania4"; then
    echo "Existing MechMania IV images:"
    docker images | grep -E "mechmania4|REPOSITORY" | head -10
else
    echo -e "${YELLOW}⚠ No MechMania IV Docker images found${NC}"
    echo "  Images need to be built"
fi
echo ""

# Check image age if they exist
echo "7. Checking image freshness..."
IMAGE_OUTDATED=0
for img in mechmania4:latest mechmania4-alpine:latest mechmania4-web:latest; do
    if docker images --format "{{.Repository}}:{{.Tag}} {{.CreatedAt}}" | grep -q "$img"; then
        CREATED=$(docker images --format "{{.Repository}}:{{.Tag}} {{.CreatedAt}}" | grep "$img" | awk '{print $2, $3, $4}')
        echo "  $img created: $CREATED"

        # Check if image is older than source code
        IMAGE_DATE=$(docker inspect --format='{{.Created}}' "$img" 2>/dev/null | cut -d'T' -f1 || echo "unknown")
        if [ "$IMAGE_DATE" != "unknown" ]; then
            # Check if any source files are newer than image
            NEWER_FILES=$(find team/src -name "*.C" -o -name "*.h" -newer <(docker inspect --format='{{.Created}}' "$img" 2>/dev/null | xargs -I{} date -d {} +%s | xargs -I{} date -d @{} 2>/dev/null) 2>/dev/null | wc -l || echo "0")
            if [ "$NEWER_FILES" -gt 0 ]; then
                echo -e "${YELLOW}  ⚠ $NEWER_FILES source files newer than image${NC}"
                ((IMAGE_OUTDATED++))
            fi
        fi
    else
        echo -e "${YELLOW}  ⚠ $img not found${NC}"
        ((IMAGE_OUTDATED++))
    fi
done
echo ""

# Summary
echo "========================================"
echo "Verification Summary"
echo "========================================"
echo ""

if [ $IMAGE_OUTDATED -gt 0 ]; then
    echo -e "${YELLOW}⚠ Docker images are outdated or missing${NC}"
    echo ""
    echo "To rebuild all images, run:"
    echo "  docker build -f Dockerfile -t mechmania4:latest ."
    echo "  docker build -f Dockerfile.alpine -t mechmania4-alpine:latest ."
    echo "  docker build -f Dockerfile.web -t mechmania4-web:latest ."
    echo ""
    echo "Or use docker-compose:"
    echo "  docker-compose --profile quick build"
    echo "  docker-compose --profile tournament build"
    echo "  docker-compose --profile web build"
    echo ""
else
    echo -e "${GREEN}✓ All Docker images are up-to-date${NC}"
    echo ""
fi

echo "To test the images:"
echo "  # Quick headless test (Alpine):"
echo "  docker run --rm mechmania4-alpine:latest"
echo ""
echo "  # Web interface test:"
echo "  docker run -d -p 6080:6080 --name mm4-test mechmania4-web:latest"
echo "  # Then open: http://localhost:6080/vnc.html"
echo "  # Stop with: docker stop mm4-test && docker rm mm4-test"
echo ""

echo "Docker setup verification complete!"
