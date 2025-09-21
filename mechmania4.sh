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

build_images() {
    echo -e "${YELLOW}Building Docker images...${NC}"
    docker build -t $IMAGE_NAME -f Dockerfile .
    docker build -t $WEB_IMAGE_NAME -f Dockerfile.web .
    docker build -t $ALPINE_IMAGE_NAME -f Dockerfile.alpine .
    echo -e "${GREEN}Build complete!${NC}"
}

show_menu() {
    echo ""
    echo "Choose an option:"
    echo "  1) Quick Game (X11 graphics)"
    echo "  2) Web Browser Game (http://localhost:6080)"
    echo "  3) Tournament Mode (multiple matches)"
    echo "  4) Headless Test (no graphics)"
    echo "  5) Development Mode (with file watching)"
    echo "  6) Build Docker Images"
    echo "  7) Download Dependencies (for offline builds)"
    echo "  8) Exit"
    echo ""
    read -p "Enter choice [1-8]: " choice
}

run_x11_game() {
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
        DISPLAY_ARG="-e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix:rw"
    fi

    docker run -it --rm \
        $DISPLAY_ARG \
        --network host \
        $IMAGE_NAME \
        ./run_game.sh
}

run_web_game() {
    echo -e "${GREEN}Starting web-based game...${NC}"
    echo -e "${YELLOW}Open your browser to: http://localhost:6080/vnc.html${NC}"

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
    echo -e "${GREEN}Starting tournament mode...${NC}"
    docker-compose --profile tournament up
}

run_headless() {
    echo -e "${GREEN}Running headless test...${NC}"
    docker run -it --rm $ALPINE_IMAGE_NAME ./quick_test.sh
}

run_dev_mode() {
    echo -e "${GREEN}Starting development mode...${NC}"
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