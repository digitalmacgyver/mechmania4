#!/bin/bash
# MechMania IV Docker Launcher for Linux/macOS
# Makes it easy to run MechMania IV with Docker

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
IMAGE_NAME="mechmania4"
WEB_IMAGE_NAME="mechmania4-web"
ALPINE_IMAGE_NAME="mechmania4-alpine"

# Version marker - increment this when Docker images need rebuilding
DOCKER_BUILD_VERSION="2024.01.02"

# Functions
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${GREEN}  MechMania IV: The Vinyl Frontier${NC}"
    echo -e "${BLUE}========================================${NC}"
}

check_docker() {
    if ! command -v docker &> /dev/null; then
        echo -e "${RED}Docker is not installed!${NC}"
        echo "Please install Docker from https://docs.docker.com/get-docker/"
        exit 1
    fi
}

check_images() {
    # Check if images exist
    if ! docker images | grep -q "^${IMAGE_NAME} "; then
        echo -e "${YELLOW}Docker image '${IMAGE_NAME}' not found!${NC}"
        echo -e "${YELLOW}Building images first (this will take 5-10 minutes)...${NC}"
        build_images
    fi
}

check_web_image() {
    if ! docker images | grep -q "^${WEB_IMAGE_NAME} "; then
        echo -e "${YELLOW}Docker image '${WEB_IMAGE_NAME}' not found!${NC}"
        echo -e "${YELLOW}Building web image (this will take 5-10 minutes)...${NC}"
        docker build -t $WEB_IMAGE_NAME -f Dockerfile.web .
        echo -e "${GREEN}Web image build complete!${NC}"
    fi
}

check_alpine_image() {
    if ! docker images | grep -q "^${ALPINE_IMAGE_NAME} "; then
        echo -e "${YELLOW}Docker image '${ALPINE_IMAGE_NAME}' not found!${NC}"
        echo -e "${YELLOW}Building Alpine image (this will take a few minutes)...${NC}"
        docker build -t $ALPINE_IMAGE_NAME -f Dockerfile.alpine .
        echo -e "${GREEN}Alpine image build complete!${NC}"
    fi
}

build_images() {
    echo -e "${YELLOW}Building Docker images...${NC}"
    echo -e "${YELLOW}This will take 5-10 minutes the first time.${NC}"
    echo ""
    echo -e "${BLUE}Note: Rebuild images when:${NC}"
    echo "  - You've pulled new code from git"
    echo "  - You see 'no such file or directory' errors"
    echo "  - The game fails to start properly"
    echo ""

    echo -e "\n${YELLOW}[1/3] Building standard image...${NC}"
    docker build -t $IMAGE_NAME -f Dockerfile .

    echo -e "\n${YELLOW}[2/3] Building web image...${NC}"
    docker build -t $WEB_IMAGE_NAME -f Dockerfile.web .

    echo -e "\n${YELLOW}[3/3] Building Alpine image...${NC}"
    docker build -t $ALPINE_IMAGE_NAME -f Dockerfile.alpine .

    echo -e "${GREEN}All images built successfully!${NC}"
    echo -e "${GREEN}Build version: ${DOCKER_BUILD_VERSION}${NC}"
}

show_menu() {
    echo ""
    echo "Choose an option:"
    echo "  1) Quick Game (X11 graphics)"
    echo "  2) Web Browser Game (http://localhost:6080)"
    echo "  3) Tournament Mode (multiple matches)"
    echo "  4) Headless Test (no graphics)"
    echo "  5) Development Mode (with file watching)"
    echo "  6) Build/Rebuild Docker Images"
    echo "  7) Download Dependencies (for offline builds)"
    echo "  8) Exit"
    echo ""
    read -p "Enter choice [1-8]: " choice
}

run_x11_game() {
    check_images
    echo -e "${GREEN}Starting game with X11 graphics...${NC}"

    # Check for X11 on macOS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if ! command -v XQuartz &> /dev/null; then
            echo -e "${YELLOW}XQuartz not found. Install from https://www.xquartz.org/${NC}"
            echo "After installing, run: open -a XQuartz"
            echo "Then in XQuartz preferences, enable 'Allow connections from network clients'"
            exit 1
        fi
        DISPLAY_ARG="-e DISPLAY=host.docker.internal:0"
    else
        # Allow X11 connections on Linux
        xhost +local:docker 2>/dev/null || true
        DISPLAY_ARG="-e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix:rw"
    fi

    # Run the container and capture the exit code
    docker run -it --rm \
        $DISPLAY_ARG \
        --network host \
        $IMAGE_NAME \
        ./run_game.sh

    # Check if the container failed with a common error
    if [ $? -ne 0 ]; then
        echo ""
        echo -e "${RED}========================================${NC}"
        echo -e "${RED}Container failed to start!${NC}"
        echo -e "${RED}========================================${NC}"
        echo ""
        echo -e "${YELLOW}This often happens when Docker images are outdated.${NC}"
        echo -e "${YELLOW}Your local images may have been built from an older version.${NC}"
        echo ""
        echo -e "${GREEN}To fix this problem:${NC}"
        echo -e "${GREEN}1. Run this script again: ./mechmania4.sh${NC}"
        echo -e "${GREEN}2. Choose option 6 to rebuild Docker images${NC}"
        echo -e "${GREEN}3. Try your command again${NC}"
        echo ""
        echo -e "${BLUE}Alternatively, rebuild manually:${NC}"
        echo -e "  docker build -t mechmania4 ."
        echo -e "  docker build -f Dockerfile.alpine -t mechmania4-alpine ."
        echo -e "  docker build -f Dockerfile.web -t mechmania4-web ."
        echo ""
        return 1
    fi
}

run_web_game() {
    check_web_image
    echo -e "${GREEN}Starting web-based game...${NC}"
    echo -e "${YELLOW}Open your browser to: http://localhost:6080/vnc.html${NC}"

    # Stop any existing container
    docker stop mechmania4-web 2>/dev/null || true
    docker rm mechmania4-web 2>/dev/null || true

    docker run -d --rm \
        --name mechmania4-web \
        -p 6080:6080 \
        -p 5900:5900 \
        -p 2323:2323 \
        $WEB_IMAGE_NAME

    # Wait a moment for services to start
    sleep 3

    # Try to open browser
    if command -v xdg-open &> /dev/null; then
        xdg-open http://localhost:6080/vnc.html
    elif command -v open &> /dev/null; then
        open http://localhost:6080/vnc.html
    fi

    echo -e "${GREEN}Web interface started!${NC}"
    echo "Press Ctrl+C to stop..."

    # Wait for user to stop
    trap "docker stop mechmania4-web" INT
    docker logs -f mechmania4-web
}

run_tournament() {
    check_images
    echo -e "${GREEN}Starting tournament mode...${NC}"

    # Check if docker-compose exists
    if ! command -v docker-compose &> /dev/null; then
        echo -e "${RED}docker-compose not found!${NC}"
        echo "Install with: sudo apt-get install docker-compose (Linux)"
        echo "Or: brew install docker-compose (macOS)"
        exit 1
    fi

    docker-compose --profile tournament up
}

run_headless() {
    check_alpine_image
    echo -e "${GREEN}Running headless test...${NC}"
    docker run -it --rm $ALPINE_IMAGE_NAME ./quick_test.sh

    # Check if the container failed
    if [ $? -ne 0 ]; then
        echo ""
        echo -e "${RED}========================================${NC}"
        echo -e "${RED}Headless test failed!${NC}"
        echo -e "${RED}========================================${NC}"
        echo ""
        echo -e "${YELLOW}If you see 'no such file or directory' errors,${NC}"
        echo -e "${YELLOW}your Docker images are likely outdated.${NC}"
        echo ""
        echo -e "${GREEN}To fix: Choose option 6 to rebuild Docker images${NC}"
        echo ""
        return 1
    fi
}

run_dev_mode() {
    check_images
    echo -e "${GREEN}Starting development mode...${NC}"

    # Check if docker-compose exists
    if ! command -v docker-compose &> /dev/null; then
        echo -e "${RED}docker-compose not found!${NC}"
        echo "Install with: sudo apt-get install docker-compose"
        exit 1
    fi

    docker-compose --profile dev up
}

download_deps() {
    echo -e "${YELLOW}Downloading dependencies for offline builds...${NC}"
    if [ -f vendor/download-dependencies.sh ]; then
        chmod +x vendor/download-dependencies.sh
        ./vendor/download-dependencies.sh
    else
        echo -e "${RED}vendor/download-dependencies.sh not found!${NC}"
    fi
}

cleanup() {
    echo -e "${YELLOW}Cleaning up...${NC}"
    docker-compose down 2>/dev/null || true
    docker stop mechmania4-web 2>/dev/null || true
    echo -e "${GREEN}Cleanup complete!${NC}"
}

# Main script
print_header
check_docker

# Handle command line arguments
case "$1" in
    quick|x11)
        run_x11_game
        ;;
    web)
        run_web_game
        ;;
    tournament)
        run_tournament
        ;;
    test)
        run_headless
        ;;
    dev)
        run_dev_mode
        ;;
    build)
        build_images
        ;;
    deps)
        download_deps
        ;;
    cleanup)
        cleanup
        ;;
    *)
        while true; do
            show_menu
            case $choice in
                1) run_x11_game; break ;;
                2) run_web_game; break ;;
                3) run_tournament; break ;;
                4) run_headless; break ;;
                5) run_dev_mode; break ;;
                6) build_images ;;
                7) download_deps ;;
                8) echo "Goodbye!"; exit 0 ;;
                *) echo -e "${RED}Invalid choice!${NC}" ;;
            esac
        done
        ;;
esac