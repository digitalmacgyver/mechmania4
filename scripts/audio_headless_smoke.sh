#!/usr/bin/env bash
# Runs the SDL observer and audio probe in headless mode to verify that
# playlist initialization and effect scheduling continue to emit logs.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
KEEP_LOGS=0

usage() {
  cat <<'EOF'
Usage: scripts/audio_headless_smoke.sh [--keep-logs] [build_dir]

  --keep-logs  Preserve the temporary log files instead of deleting them.
  build_dir    Optional build directory (defaults to ./build).
EOF
}

POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --keep-logs)
      KEEP_LOGS=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      POSITIONAL_ARGS+=("$1")
      shift
      ;;
  esac
done

if [[ ${#POSITIONAL_ARGS[@]} -gt 0 ]]; then
  BUILD_DIR="${POSITIONAL_ARGS[0]}"
fi

PROBE_BIN="${BUILD_DIR}/mm4_audio_probe"
OBSERVER_BIN="${BUILD_DIR}/mm4obs"

if [[ ! -x "${PROBE_BIN}" ]]; then
  echo "error: ${PROBE_BIN} not found or not executable. Build the project first." >&2
  exit 1
fi

if [[ ! -x "${OBSERVER_BIN}" ]]; then
  echo "error: ${OBSERVER_BIN}" \
       "not found or not executable. Build the observer first." >&2
  exit 1
fi

TMP_PROBE_LOG="$(mktemp)"
TMP_OBS_LOG="$(mktemp)"
cleanup() {
  if [[ "${KEEP_LOGS}" -eq 0 ]]; then
    rm -f "${TMP_PROBE_LOG}" "${TMP_OBS_LOG}"
  fi
}
trap cleanup EXIT

pushd "${ROOT_DIR}" >/dev/null

echo "[audio-smoke] running mm4_audio_probe..."
"${PROBE_BIN}" >"${TMP_PROBE_LOG}" 2>&1 || {
  echo "[audio-smoke] probe failed; captured output:"
  cat "${TMP_PROBE_LOG}"
  exit 1
}

grep -q "\[audio\] tick=" "${TMP_PROBE_LOG}" || {
  echo "[audio-smoke] expected schedule log missing from probe output:"
  cat "${TMP_PROBE_LOG}"
  exit 1
}

grep -q "\[audio\] music next track=" "${TMP_PROBE_LOG}" || {
  echo "[audio-smoke] expected music log missing from probe output:"
  cat "${TMP_PROBE_LOG}"
  exit 1
}

grep -q "\[audio-probe] queued" "${TMP_PROBE_LOG}" || {
  echo "[audio-smoke] probe summary missing:"
  cat "${TMP_PROBE_LOG}"
  exit 1
}

echo "[audio-smoke] running mm4obs in dummy video mode..."
if ! timeout 6s env SDL_VIDEODRIVER=dummy "${OBSERVER_BIN}" --mute --verbose --enable-audio-test-ping \
     >"${TMP_OBS_LOG}" 2>&1; then
  echo "[audio-smoke] observer exited with non-zero status (expected if timeout fired)."
fi

grep -q "\[audio] playlist context=initialize" "${TMP_OBS_LOG}" || {
  echo "[audio-smoke] playlist initialization log missing:"
  cat "${TMP_OBS_LOG}"
  exit 1
}

grep -q "\[audio] music next track=" "${TMP_OBS_LOG}" || {
  echo "[audio-smoke] music track log missing:"
  cat "${TMP_OBS_LOG}"
  exit 1
}

echo "[audio-smoke] success. Logs captured at:"
echo "  probe -> ${TMP_PROBE_LOG}"
echo "  observer -> ${TMP_OBS_LOG}"
if [[ "${KEEP_LOGS}" -eq 0 ]]; then
  echo "  (pass --keep-logs to retain log files after this script exits)"
fi
popd >/dev/null
