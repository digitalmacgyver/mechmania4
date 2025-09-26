# MechMania IV: The Vinyl Frontier

This repository contains a revised version of the server code and Team Groogroo code for the 1998 UIUC MechMania IV: The Vinyl Frontier competition.

## Quick Start

To see the simulation on Linux:

```bash
mkdir -p build
cd build
cmake ..
make -j
../scripts/run_groogroo.sh
```

## Documentation

For detailed information, consult the following documentation:

- **[Contest Rules](docs/CONTEST_RULES.md)** - Rules and mechanics of the competition
- **[Running Contests](docs/README_RUN_CONTEST.md)** - How to build and run the code on Linux
- **[Build Instructions](docs/README_BUILD.md)** - Detailed build instructions
- **[Team API](docs/TEAM_API.md)** - API for developing teams
- **[Modernization Notes](docs/README_MODERNIZED.md)** - Details about code updates
- **[Agents Guide](AGENTS.md)** - Information about AI agents

### Docker Setup

For MacOs, Windows, or Linux systems that don't want to install build tools.

- **[Docker User Guide](docs/README_DOCKER_USER.md)** - Setup and run the contest in Docker
- **[Docker Development Guide](docs/README_DOCKER_DEV.md)** - Docker containerization details

## Project Structure

### Modernized Code

An updated version of the MMIV code that can be built and run on modern Linux:

- **`build/`** - Target directory for build outputs
- **`team/src/`** - The MechMania IV server code and example team (Chrome Funkadelic)
- **`teams/`** - Implementations of alternative teams:
  - **`groogroo/`** - Team Groogroo (advanced AI)
  - **`chromefunk/`** - The example Chrome Funkadelic team
  - **`vortex/`** - A team built with Claude Code

### Legacy Code

- **`legacy_code/`** - A snapshot of the original 1998 server code and some Team Groogroo files
  - This is not functional and included for historic interest only
  - See `legacy_code/README.txt` for more details

### Docker Configuration

Configuration and setup files for running a dockerized version of the contest.

For instrutions on getting docker running, see [Docker Instructions](docs/README_DOCKER_USER.md).

For details of the Docker archival approach for this code, see [README_DOCKER_DEV.md](docs/README_DOCKER_DEV.md).

## Getting Started

1. Clone this repository
2. Follow the [build instructions](docs/README_BUILD.md)
3. Check out the [contest rules](docs/CONTEST_RULES.md) to understand the game
4. Try running the [sample contest](docs/README_RUN_CONTEST.md)
5. Develop your own team using the [Team API](docs/TEAM_API.md)

## About MechMania IV

MechMania IV: The Vinyl Frontier was a programming contest held at the University of Illinois at Urbana-Champaign in 1998. Teams competed by programming AI agents to control spaceships in a space-based strategy game involving resource collection, combat, and territorial control.