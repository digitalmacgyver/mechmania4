#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="/tmp/mm_archive"

mkdir -p "${OUTPUT_DIR}"

concat_files() {
  local output_file="$1"
  shift

  local files=("$@")
  for file in "${files[@]}"; do
    if [[ ! -f "${file}" ]]; then
      echo "Required file not found: ${file}" >&2
      exit 1
    fi
  done

  cat "${files[@]}" > "${output_file}"
}

docs_files=(
  "${REPO_ROOT}/docs/TEAM_API.md"
  "${REPO_ROOT}/docs/CONTEST_DAMAGE_FOR_DEVS.md"
  "${REPO_ROOT}/docs/CONTEST_NAVIGATION_FOR_DEVS.md"
  "${REPO_ROOT}/docs/CONTEST_PHYSICS_FOR_DEVS.md"
  "${REPO_ROOT}/docs/CONTEST_RULES.md"
  "${REPO_ROOT}/docs/NEW_COLLISION_ENGINE.md"
)
concat_files "${OUTPUT_DIR}/docs.md" "${docs_files[@]}"

args_consts_files=(
  "${REPO_ROOT}/team/src/GameConstants.h"
  "${REPO_ROOT}/team/src/GameConstants.C"
  "${REPO_ROOT}/team/src/ArgumentParser.h"
  "${REPO_ROOT}/team/src/ArgumentParser.C"
)
concat_files "${OUTPUT_DIR}/ArgsConstsCombined.cpp" "${args_consts_files[@]}"

module_names=(
  "Asteroid"
  "CollisionTypes"
  "Server"
  "Ship"
  "Station"
  "Thing"
  "World"
)

for module in "${module_names[@]}"; do
  concat_files "${OUTPUT_DIR}/${module}Combined.cpp" \
    "${REPO_ROOT}/team/src/${module}.h" \
    "${REPO_ROOT}/team/src/${module}.C"
done

coord_files=(
  "${REPO_ROOT}/team/src/Coord.h"
  "${REPO_ROOT}/team/src/Coord.C"
  "${REPO_ROOT}/team/src/Traj.h"
  "${REPO_ROOT}/team/src/Traj.C"
  "${REPO_ROOT}/team/src/PhysicsUtils.h"
  "${REPO_ROOT}/team/src/PhysicsUtils.C"
)
concat_files "${OUTPUT_DIR}/CoordsCombined.cpp" "${coord_files[@]}"

echo "Archive artifacts written to ${OUTPUT_DIR}"
