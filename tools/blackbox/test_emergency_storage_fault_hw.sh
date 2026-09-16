#!/usr/bin/env bash
set -Eeuo pipefail

# Destructive operations are confined to a newly-created loopback image under
# /var/tmp. The production database, production port and active systemd unit
# are never addressed by this test.

if [[ "${1:-}" != "--confirm-isolated-loopback" ]]; then
    echo "usage: sudo $0 --confirm-isolated-loopback" >&2
    exit 2
fi
if [[ "$EUID" -ne 0 ]]; then
    echo "error: run with sudo; loopback mount is required" >&2
    exit 2
fi

for command in mkfs.ext4 mount umount mountpoint fallocate dd curl start-stop-daemon python3 ss systemctl; do
    command -v "$command" >/dev/null || {
        echo "error: required command not found: $command" >&2
        exit 2
    }
done
id blackbox >/dev/null 2>&1 || {
    echo "error: blackbox service account not found" >&2
    exit 2
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
HUB_SCRIPT="$SCRIPT_DIR/emergency_event_hub.py"
[[ -f "$HUB_SCRIPT" ]] || {
    echo "error: emergency_event_hub.py not found beside this script" >&2
    exit 2
}

TEST_ROOT="$(mktemp -d /var/tmp/efis-emergency-storage-fault.XXXXXX)"
case "$TEST_ROOT" in
    /var/tmp/efis-emergency-storage-fault.*) ;;
    *) echo "error: unsafe temporary path: $TEST_ROOT" >&2; exit 2 ;;
esac
# mktemp creates the directory as 0700/root.  The isolated hub deliberately
# runs as the production service account, so grant only that group traversal;
# without this the hub cannot reach its token file or mounted database.
chown root:blackbox "$TEST_ROOT"
chmod 0710 "$TEST_ROOT"

MOUNT_DIR="$TEST_ROOT/mnt"
IMAGE_FILE="$TEST_ROOT/storage.img"
TOKEN_FILE="$TEST_ROOT/tokens.json"
EVENT_ONE="$TEST_ROOT/event-1.json"
EVENT_TWO="$TEST_ROOT/event-2.json"
RESPONSE_FILE="$TEST_ROOT/response.json"
LOG_FILE="$TEST_ROOT/hub.log"
PID_FILE="$TEST_ROOT/hub.pid"
DATABASE="$MOUNT_DIR/emergency-events.sqlite3"
PORT=28094
BASE_URL="http://127.0.0.1:$PORT"
SOURCE_NODE="BLACKBOX-HW-TEST"
SESSION_ID="storage-fault-$(date -u +%Y%m%dT%H%M%SZ)-$$"
TEST_TOKEN="$(cat /proc/sys/kernel/random/uuid)$(cat /proc/sys/kernel/random/uuid)"
REPORT_FILE="/var/tmp/efis-emergency-storage-fault-report-$(date -u +%Y%m%dT%H%M%SZ).json"
HUB_RUNNING=false
MOUNTED=false
PRODUCTION_STATE_BEFORE="$(systemctl is-active efis-emergency-event-hub.service 2>/dev/null || true)"

if ss -H -ltn "sport = :$PORT" | grep -q .; then
    echo "error: isolated test port $PORT is already in use" >&2
    exit 2
fi

stop_hub() {
    if [[ "$HUB_RUNNING" == true && -s "$PID_FILE" ]]; then
        start-stop-daemon --stop --pidfile "$PID_FILE" --retry TERM/3/KILL/2 \
            --remove-pidfile >/dev/null 2>&1 || true
    fi
    HUB_RUNNING=false
}

cleanup() {
    set +e
    if [[ "$MOUNTED" == true ]]; then
        rm -f -- "$MOUNT_DIR/fill.bin"
    fi
    stop_hub
    if [[ "$MOUNTED" == true ]] && mountpoint -q "$MOUNT_DIR"; then
        umount -- "$MOUNT_DIR"
    fi
    rm -f -- "$IMAGE_FILE" "$TOKEN_FILE" "$EVENT_ONE" "$EVENT_TWO" \
        "$RESPONSE_FILE" "$LOG_FILE" "$PID_FILE"
    rmdir -- "$MOUNT_DIR" 2>/dev/null || true
    rmdir -- "$TEST_ROOT" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

start_hub() {
    rm -f -- "$PID_FILE"
    start-stop-daemon --start --background --make-pidfile --pidfile "$PID_FILE" \
        --chuid blackbox:blackbox --startas /usr/bin/python3 -- \
        "$HUB_SCRIPT" --bind 127.0.0.1 --port "$PORT" \
        --database "$DATABASE" --tokens "$TOKEN_FILE" \
        >>"$LOG_FILE" 2>&1
    HUB_RUNNING=true
}

wait_for_code() {
    local expected="$1"
    local code="000"
    for _ in $(seq 1 50); do
        code="$(curl -sS --max-time 1 -o "$RESPONSE_FILE" -w '%{http_code}' \
            "$BASE_URL/health" || true)"
        [[ "$code" == "$expected" ]] && return 0
        sleep 0.1
    done
    echo "error: expected health HTTP $expected, received $code" >&2
    [[ -f "$LOG_FILE" ]] && tail -n 30 "$LOG_FILE" >&2
    return 1
}

post_event() {
    local event_file="$1"
    curl -sS --max-time 5 -o "$RESPONSE_FILE" -w '%{http_code}' \
        -H "Authorization: Bearer $TEST_TOKEN" \
        -H 'Content-Type: application/json' \
        --data-binary "@$event_file" "$BASE_URL/v1/emergency-events"
}

truncate -s 32M "$IMAGE_FILE"
mkfs.ext4 -q -F -m 0 "$IMAGE_FILE"
mkdir "$MOUNT_DIR"
mount -o loop,nodev,nosuid,noexec "$IMAGE_FILE" "$MOUNT_DIR"
MOUNTED=true
chown blackbox:blackbox "$MOUNT_DIR"

umask 077
printf '{"%s":"%s"}\n' "$SOURCE_NODE" "$TEST_TOKEN" >"$TOKEN_FILE"
chown blackbox:blackbox "$TOKEN_FILE"

python3 - "$EVENT_ONE" "$EVENT_TWO" "$SOURCE_NODE" "$SESSION_ID" <<'PY'
import json
import sys

for sequence, path in enumerate(sys.argv[1:3], 1):
    value = {
        "source": sys.argv[3],
        "event": {
            "schema": "efis-next.emergency-event.v1",
            "session_id": sys.argv[4],
            "event_sequence": sequence,
            "written_utc": "2026-09-13T00:00:00.000Z",
            "code": "STORAGE_FAULT_HW_TEST",
            # Stay below the protocol's 4096-character detail limit while
            # keeping each durable write large enough for the storage test.
            "detail": "isolated loopback filesystem test " + ("x" * 4000),
            "assist_state": "TEST ONLY",
            "emergency_kind": "STORAGE TEST",
            "simulation": True,
        },
    }
    with open(path, "w", encoding="utf-8") as stream:
        json.dump(value, stream, separators=(",", ":"))
PY

start_hub
wait_for_code 200

first_code="$(post_event "$EVENT_ONE")"
[[ "$first_code" == 201 ]] || {
    echo "error: initial durable write returned HTTP $first_code" >&2
    exit 1
}

available_bytes="$(df -B1 --output=avail "$MOUNT_DIR" | tail -n 1 | tr -d ' ')"
[[ "$available_bytes" =~ ^[0-9]+$ && "$available_bytes" -gt 0 ]] || {
    echo "error: invalid available byte count: $available_bytes" >&2
    exit 1
}
if ! fallocate -l "$available_bytes" "$MOUNT_DIR/fill.bin"; then
    rm -f -- "$MOUNT_DIR/fill.bin"
    # The image is fixed at 32 MiB. dd is bounded by that filesystem and the
    # expected ENOSPC is deliberately ignored.
    dd if=/dev/zero of="$MOUNT_DIR/fill.bin" bs=1M status=none || true
fi

full_code="$(post_event "$EVENT_TWO")"
[[ "$full_code" == 507 ]] || {
    echo "error: full filesystem write returned HTTP $full_code, expected 507" >&2
    exit 1
}
python3 - "$RESPONSE_FILE" <<'PY'
import json
import sys

value = json.load(open(sys.argv[1], encoding="utf-8"))
assert value["persisted"] is False
assert value["detail"] == "sqlite_full"
PY
wait_for_code 503

rm -f -- "$MOUNT_DIR/fill.bin"
sync -f "$MOUNT_DIR"
recovery_code="$(post_event "$EVENT_TWO")"
[[ "$recovery_code" == 201 ]] || {
    echo "error: recovery write returned HTTP $recovery_code" >&2
    exit 1
}
wait_for_code 200

before_kill_records="$(curl -sS --max-time 5 \
    -H "Authorization: Bearer $TEST_TOKEN" \
    "$BASE_URL/v1/emergency-events?limit=10" | \
    python3 -c 'import json,sys; print(len(json.load(sys.stdin)["events"]))')"
[[ "$before_kill_records" == 2 ]] || {
    echo "error: expected 2 records before abrupt exit, got $before_kill_records" >&2
    exit 1
}

hub_pid="$(cat "$PID_FILE")"
kill -KILL "$hub_pid"
HUB_RUNNING=false
for _ in $(seq 1 50); do
    kill -0 "$hub_pid" 2>/dev/null || break
    sleep 0.1
done
if kill -0 "$hub_pid" 2>/dev/null; then
    echo "error: isolated hub did not stop after SIGKILL" >&2
    exit 1
fi
rm -f -- "$PID_FILE"

start_hub
wait_for_code 200
after_restart_records="$(curl -sS --max-time 5 \
    -H "Authorization: Bearer $TEST_TOKEN" \
    "$BASE_URL/v1/emergency-events?limit=10" | \
    python3 -c 'import json,sys; print(len(json.load(sys.stdin)["events"]))')"
[[ "$after_restart_records" == 2 ]] || {
    echo "error: expected 2 records after abrupt restart, got $after_restart_records" >&2
    exit 1
}
PRODUCTION_STATE_AFTER="$(systemctl is-active efis-emergency-event-hub.service 2>/dev/null || true)"
[[ "$PRODUCTION_STATE_AFTER" == "$PRODUCTION_STATE_BEFORE" ]] || {
    echo "error: production service state changed unexpectedly" >&2
    exit 1
}

cat >"$REPORT_FILE" <<EOF
{
  "schema": "efis-next.emergency-storage-fault-hw-report.v1",
  "target": "$(hostname)",
  "test_filesystem": "isolated-ext4-loopback",
  "production_service_touched": false,
  "production_service_before": "$PRODUCTION_STATE_BEFORE",
  "production_service_after": "$PRODUCTION_STATE_AFTER",
  "initial_write_http": $first_code,
  "full_write_http": $full_code,
  "full_write_persisted": false,
  "health_while_full_http": 503,
  "recovery_write_http": $recovery_code,
  "records_before_sigkill": $before_kill_records,
  "records_after_restart": $after_restart_records,
  "quick_check_on_restart": "startup-pass",
  "power_loss_verified": false,
  "result": "PASS"
}
EOF

echo "PASS: isolated filesystem full, recovery and abrupt process restart"
echo "REPORT=$REPORT_FILE"
