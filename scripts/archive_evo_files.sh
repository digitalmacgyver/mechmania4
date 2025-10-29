#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="/tmp/mm_evo"

mkdir -p "${OUTPUT_DIR}"

cp "${REPO_ROOT}/teams/evo/EvoAI.h" "${OUTPUT_DIR}"
cp "${REPO_ROOT}/teams/evo/EvoAI.C" "${OUTPUT_DIR}"
cp "${REPO_ROOT}/teams/evo/ga_optimizer.py" "${OUTPUT_DIR}"

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
  "${REPO_ROOT}/docs/NEW_COLLISION_ENGINE.md"
)
concat_files "${OUTPUT_DIR}/docs.md" "${docs_files[@]}"

args_consts_files=(
  "${REPO_ROOT}/team/src/GameConstants.h"
  "${REPO_ROOT}/team/src/GameConstants.C"
  "${REPO_ROOT}/team/src/Thing.h"
  "${REPO_ROOT}/team/src/Thing.C"
  "${REPO_ROOT}/team/src/Ship.h"
  "${REPO_ROOT}/team/src/Ship.C"
  "${REPO_ROOT}/team/src/Station.h"
  "${REPO_ROOT}/team/src/Station.C"
  "${REPO_ROOT}/team/src/Coord.h"
  "${REPO_ROOT}/team/src/Coord.C"
  "${REPO_ROOT}/team/src/Traj.h"
  "${REPO_ROOT}/team/src/Traj.C"
  "${REPO_ROOT}/team/src/PhysicsUtils.h"
  "${REPO_ROOT}/team/src/PhysicsUtils.C"
)
concat_files "${OUTPUT_DIR}/ServerCombined.cpp" "${args_consts_files[@]}"

args_consts_files=(
  "${REPO_ROOT}/team/src/Team.h"
  "${REPO_ROOT}/team/src/Team.C"
  "${REPO_ROOT}/team/src/Brain.h"
  "${REPO_ROOT}/teams/groogroo/Collision.h"
  "${REPO_ROOT}/teams/groogroo/Entry.h"
  "${REPO_ROOT}/teams/groogroo/Entry.C"
  "${REPO_ROOT}/teams/groogroo/FuelTraj.h"
  "${REPO_ROOT}/teams/groogroo/MagicBag.h"
  "${REPO_ROOT}/teams/groogroo/MagicBag.C"
  "${REPO_ROOT}/teams/groogroo/GetVinyl.h"
  "${REPO_ROOT}/teams/groogroo/GetVinyl.C"
  "${REPO_ROOT}/teams/groogroo/Groogroo.h"
  "${REPO_ROOT}/teams/groogroo/Groogroo.C"
)
concat_files "${OUTPUT_DIR}/GroogrooCombined.cpp" "${args_consts_files[@]}"

echo "Archive artifacts written to ${OUTPUT_DIR}"
