# Repository Guidelines

## Project Structure & Module Organization
- `team/src`: Primary C++17 sources and headers. Entrypoints: `mm4serv` (server), `mm4team` (team AI), `mm4obs` (observer/graphics). Assets: `gfx/`, `graphics.reg`.
- `teams/<name>`: Alternative AI implementations (e.g., `groogroo/`, `vortex/`) compiled into separate binaries.
- `build/`: CMake build output, helper scripts (`run_game.sh`, `quick_test.sh`), and logs.
- `legacy_code/`: Historical archive; keep unchanged.
- `.github/workflows/`: CI for Docker builds and basic test run.

## Build, Test, and Development Commands
- Configure + build (SDL2 graphics on):
  - `cmake -S . -B build -DBUILD_WITH_GRAPHICS=ON -DUSE_SDL2=ON`
  - `cmake --build build -j`
- Run locally (observer UI):
  - `cd build && ./run_game.sh` (use `./run_game.sh groogroo` to pit Groogroo vs ChromeFunk)
- Headless smoke test (CI parity):
  - `cd build && ./quick_test.sh` (generates `test_*.log` and prints last scores)
- Docker quick test:
  - `docker build -f Dockerfile.alpine -t mm4:alpine . && docker run --rm mm4:alpine ./quick_test.sh`

## Coding Style & Naming Conventions
- Language: C++17. Warnings enabled (`-Wall -Wextra`).
- Indentation: 2 spaces; braces on next line for functions/classes.
- Files: `CamelCase.C` for sources, `CamelCase.h` for headers.
- Symbols: Classes `CamelCase`, methods `CamelCase`, constants `UPPER_SNAKE_CASE`.
- Keep changes minimal in legacy files; prefer localized diffs. No linter required; optional `clang-format` on touched lines only.

## Testing Guidelines
- Framework: integration-only for now. Use `build/quick_test.sh` as the smoke test.
- Logs: check `build/test_server.log`, `test_vortex.log`, `test_chrome.log` for regressions.
- Additions: when changing game logic or networking, include a command snippet to reproduce results in the PR.

## Commit & Pull Request Guidelines
- Commits: imperative, concise, scoped. Example: `Fix team color assignment`.
- PRs: clear description, rationale, reproduction steps, and linked issues. Include screenshots/GIFs for observer/UI changes.
- CI: ensure GitHub Actions is green (Docker images build; Alpine quick test runs).
- Review: keep diffs focused; note any asset or protocol changes explicitly.

## Security & Configuration Tips
- Ports: server defaults to `2323` (some scripts use `2626` for tests).
- Deps (local): `cmake`, `libsdl2-dev`, `libsdl2-image-dev`, `libsdl2-ttf-dev`.
- Containers run as non-root; prefer Docker for reproducible builds and long-term preservation.

## Agent Capabilities Note
- Agents can perform git operations (`checkout`, `merge`, `push`) when explicitly asked. Earlier assumptions that this was disallowed were incorrect, confirmed through recent usage.
