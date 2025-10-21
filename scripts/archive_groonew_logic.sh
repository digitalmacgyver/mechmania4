#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="/tmp/mm_groonew"

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

# Same docs as archive_server_logic.sh
docs_files=(
  "${REPO_ROOT}/docs/TEAM_API.md"
  "${REPO_ROOT}/docs/CONTEST_DAMAGE_FOR_DEVS.md"
  "${REPO_ROOT}/docs/CONTEST_NAVIGATION_FOR_DEVS.md"
  "${REPO_ROOT}/docs/CONTEST_PHYSICS_FOR_DEVS.md"
  "${REPO_ROOT}/docs/CONTEST_RULES.md"
  "${REPO_ROOT}/docs/NEW_COLLISION_ENGINE.md"
)
concat_files "${OUTPUT_DIR}/docs.md" "${docs_files[@]}"

# Utils.cpp - combination of groonew and team/src files
utils_files=(
  "${REPO_ROOT}/teams/groonew/Collision.h"
  "${REPO_ROOT}/teams/groonew/DumbThing.h"
  "${REPO_ROOT}/teams/groonew/DumbThing.C"
  "${REPO_ROOT}/teams/groonew/ReturnToBase.h"
  "${REPO_ROOT}/teams/groonew/ReturnToBase.C"
  "${REPO_ROOT}/teams/groonew/PathInfo.h"
  "${REPO_ROOT}/team/src/Brain.h"
  "${REPO_ROOT}/team/src/Team.h"
  "${REPO_ROOT}/team/src/Team.C"
  "${REPO_ROOT}/team/src/GameConstants.h"
  "${REPO_ROOT}/team/src/World.h"
)
concat_files "${OUTPUT_DIR}/Utils.cpp" "${utils_files[@]}"

# CoordsCombined.cpp - same as archive_server_logic.sh
coord_files=(
  "${REPO_ROOT}/team/src/Coord.h"
  "${REPO_ROOT}/team/src/Coord.C"
  "${REPO_ROOT}/team/src/Traj.h"
  "${REPO_ROOT}/team/src/Traj.C"
  "${REPO_ROOT}/team/src/PhysicsUtils.h"
  "${REPO_ROOT}/team/src/PhysicsUtils.C"
)
concat_files "${OUTPUT_DIR}/CoordsCombined.cpp" "${coord_files[@]}"

# ShipCombined.cpp - same as archive_server_logic.sh
concat_files "${OUTPUT_DIR}/ShipCombined.cpp" \
  "${REPO_ROOT}/team/src/Ship.h" \
  "${REPO_ROOT}/team/src/Ship.C"

# ThingCombined.cpp - same as archive_server_logic.sh
concat_files "${OUTPUT_DIR}/ThingCombined.cpp" \
  "${REPO_ROOT}/team/src/Thing.h" \
  "${REPO_ROOT}/team/src/Thing.C"

# Groonew-specific combined files (PathInfo already in Utils.cpp, but need PathInfo.C)
# Actually, PathInfo.h is in Utils.cpp, so we combine PathInfo.h and PathInfo.C
concat_files "${OUTPUT_DIR}/PathInfoCombined.cpp" \
  "${REPO_ROOT}/teams/groonew/PathInfo.h" \
  "${REPO_ROOT}/teams/groonew/PathInfo.C"

concat_files "${OUTPUT_DIR}/PathfindingCombined.cpp" \
  "${REPO_ROOT}/teams/groonew/Pathfinding.h" \
  "${REPO_ROOT}/teams/groonew/Pathfinding.C"

concat_files "${OUTPUT_DIR}/MagicBagCombined.cpp" \
  "${REPO_ROOT}/teams/groonew/MagicBag.h" \
  "${REPO_ROOT}/teams/groonew/MagicBag.C"

concat_files "${OUTPUT_DIR}/GetVinylCombined.cpp" \
  "${REPO_ROOT}/teams/groonew/GetVinyl.h" \
  "${REPO_ROOT}/teams/groonew/GetVinyl.C"

concat_files "${OUTPUT_DIR}/GroonewCombined.cpp" \
  "${REPO_ROOT}/teams/groonew/Groonew.h" \
  "${REPO_ROOT}/teams/groonew/Groonew.C"

echo "Groonew archive artifacts written to ${OUTPUT_DIR}"