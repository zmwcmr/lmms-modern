#!/usr/bin/env bash
# Start a private Xvfb display and run one command inside it.
#
# Usage:
#   tests/stress/gui/xvfb_session.sh -- ./build/lmms --config ...
#
# Requires: Xvfb (openbox is optional but recommended for plugin windows).

set -euo pipefail

DISPLAY_NUM="${XVFB_DISPLAY_NUM:-99}"
SCREEN="${XVFB_SCREEN:-1920x1080x24}"
export DISPLAY=":${DISPLAY_NUM}"
export QT_QPA_PLATFORM=xcb

if ! command -v Xvfb >/dev/null 2>&1; then
    echo "Xvfb not found; install xorg-x11-server-Xvfb" >&2
    exit 2
fi

Xvfb "${DISPLAY}" -screen 0 "${SCREEN}" -nolisten tcp >/tmp/opencode/xvfb.log 2>&1 &
XVFB_PID=$!

WINDOW_MANAGER_PID=""
cleanup() {
    if [ -n "${WINDOW_MANAGER_PID}" ]; then
        kill "${WINDOW_MANAGER_PID}" 2>/dev/null || true
    fi
    kill "${XVFB_PID}" 2>/dev/null || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    if xdpyinfo >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

if command -v openbox >/dev/null 2>&1; then
    openbox >/tmp/opencode/openbox.log 2>&1 &
    WINDOW_MANAGER_PID=$!
fi

if [ "$#" -gt 0 ] && [ "$1" = "--" ]; then
    shift
fi
"$@"
