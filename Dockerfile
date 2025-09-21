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
COPY --from=builder /build/build/*.sh .
COPY --from=builder /build/team/src/gfx ./gfx
COPY --from=builder /build/team/src/graphics.reg .

# Set ownership
RUN chown -R mechmania:mechmania /home/mechmania/game

USER mechmania
WORKDIR /home/mechmania/game

# Default command shows help
CMD ["bash", "-c", "echo 'MechMania IV: The Vinyl Frontier\n\nAvailable commands:\n  ./mm4serv              - Start server\n  ./mm4obs               - Start observer\n  ./mm4team              - Start team (Chrome Funkadelic)\n  ./mm4team_groogroo     - Start team (Groogroo)\n  ./mm4team_vortex       - Start team (Vortex)\n  ./run_game.sh          - Run a complete match\n  ./quick_test.sh        - Quick headless test\n\nFor graphics, you need X11 forwarding or use the web version.'"]