# MechMania IV: The Vinyl Frontier - Standard Docker Build
# This Dockerfile builds MechMania IV with all components

FROM ubuntu:22.04 AS builder

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-ttf-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
WORKDIR /build
COPY . .

# Build the project
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Runtime stage - smaller image for running
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install only runtime dependencies
RUN apt-get update && apt-get install -y \
    libsdl2-2.0-0 \
    libsdl2-image-2.0-0 \
    libsdl2-ttf-2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -s /bin/bash mechmania

# Copy built binaries and resources
WORKDIR /home/mechmania/game
COPY --from=builder /build/build/mm4serv .
COPY --from=builder /build/build/mm4obs .
COPY --from=builder /build/build/mm4team* .
# Copy graphics resources directly from source (not symlinks)
COPY --from=builder /build/team/src/gfx ./gfx
COPY --from=builder /build/team/src/graphics.reg .

# Create shell scripts using heredoc for better formatting
RUN cat > run_game.sh << 'EOF'
#!/bin/bash
echo "Starting MechMania IV game..."
echo "Starting server..."
./mm4serv -p2323 -h127.0.0.1 &
SERVER_PID=$!
sleep 2
echo "Starting observer..."
./mm4obs -h127.0.0.1 -p2323 -G -R &
OBS_PID=$!
sleep 1
echo "Starting Team 1 (Chrome Funkadelic)..."
./mm4team -p2323 -h127.0.0.1 &
TEAM1_PID=$!
sleep 1
echo "Starting Team 2 (Groogroo)..."
# Default to Groogroo vs Chrome Funkadelic
if [ "$1" = "vortex" ]; then
    echo "  Using Team Vortex instead of Groogroo"
    ./mm4team_vortex -p2323 -h127.0.0.1 &
elif [ "$1" = "chrome" ]; then
    echo "  Using Chrome Funkadelic 2 instead of Groogroo"
    ./mm4team -p2323 -h127.0.0.1 &
else
    # Default: Groogroo
    ./mm4team_groogroo -p2323 -h127.0.0.1 &
fi
TEAM2_PID=$!
echo "Game started! Press Ctrl+C to stop."
trap "kill $SERVER_PID $OBS_PID $TEAM1_PID $TEAM2_PID 2>/dev/null; exit" INT
wait
EOF
RUN chmod +x run_game.sh

RUN cat > quick_test.sh << 'EOF'
#!/bin/bash
echo "Running quick headless test..."
./mm4serv -p2323 -h127.0.0.1 &
SERVER_PID=$!
sleep 1
./mm4team -p2323 -h127.0.0.1 &
TEAM1_PID=$!
./mm4team -p2323 -h127.0.0.1 &
TEAM2_PID=$!
sleep 5
kill $SERVER_PID $TEAM1_PID $TEAM2_PID 2>/dev/null
echo "Test completed!"
EOF
RUN chmod +x quick_test.sh

# Set ownership
RUN chown -R mechmania:mechmania /home/mechmania/game

USER mechmania
WORKDIR /home/mechmania/game

# Default command shows help
CMD ["bash", "-c", "echo 'MechMania IV: The Vinyl Frontier\n\nAvailable commands:\n  ./mm4serv              - Start server\n  ./mm4obs               - Start observer\n  ./mm4team              - Start team (Chrome Funkadelic)\n  ./mm4team_groogroo     - Start team (Groogroo)\n  ./mm4team_vortex       - Start team (Vortex)\n  ./run_game.sh          - Run a complete match\n  ./quick_test.sh        - Quick headless test\n\nFor graphics, you need X11 forwarding or use the web version.'"]