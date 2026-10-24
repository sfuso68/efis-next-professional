#!/bin/sh
# Offline reference only: no network, services, RF or avionics endpoints.
set -eu
SIL_ROOT=/home/sfuso/avionix-sil-r114-20261025
SIL_IMAGE=sha256:db3ff2e1800a8581e2c48a27c3995339d47bdf046da21c7627accd3d51053a93
SIL_SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
if [ "$#" -ne 3 ]; then
    echo 'usage: sh run_serverha_world_sil.sh SCENARIO NEW_SESSION_NAME SECONDS' >&2
    exit 2
fi
case "$1" in cruise|turn|engine-out|sensor-loss) ;; *) exit 2;; esac
case "$2" in ''|*[!a-zA-Z0-9_-]*) exit 2;; esac
test "${#2}" -le 64
case "$3" in ''|*[!0-9]*) exit 2;; esac
test "$3" -ge 1 && test "$3" -le 3600
test -d "$SIL_ROOT/runtime"
test -d "$SIL_ROOT/runs"
test ! -e "$SIL_ROOT/runs/$2"
test -f "$SIL_SOURCE/config/simulation/treviso-belluno-world-reference.json"
exec docker run --rm --pull never --network none --cpus=2 --memory=768m \
    --pids-limit=64 --cap-drop=ALL --security-opt no-new-privileges \
    --read-only --tmpfs /tmp:rw,noexec,nosuid,size=64m \
    -u "$(id -u):$(id -g)" -e PYTHONPATH=/runtime:/source -e JSBSIM_DEBUG=0 \
    -v "$SIL_ROOT/runtime:/runtime:ro" -v "$SIL_SOURCE:/source:ro" \
    -v "$SIL_ROOT/runs:/runs" -w /source "$SIL_IMAGE" \
    python -B -m tools.simulation.jsbsim_session --directory "/runs/$2" \
    --session-id "$2" --scenario "$1" --seconds "$3" \
    --world /source/config/simulation/treviso-belluno-world-reference.json
