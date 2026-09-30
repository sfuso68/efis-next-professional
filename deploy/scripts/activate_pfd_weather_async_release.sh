#!/usr/bin/env bash
# Activate the prepared PFD weather release, prove rollback, and exercise the
# asynchronous writer with injected 400 ms I/O. DEVELOPMENT/NON-CERT bench only.
set -Eeuo pipefail

operator=efis
operator_uid=$(id -u "$operator")
runtime_dir=/run/user/$operator_uid
release_id=pfdrec-3c7bb4d-weather-async-v1
candidate=/home/efis/efis-next-releases/$release_id
staged=/home/efis/efis-release-pfd-weather-stage/release-$release_id
rollback=/home/efis/efis-next-releases/pfdrec-7051a47-retention-v1
current=/home/efis/efis-next-current
validator=/usr/local/lib/efis-next/validate_release_manifest.py
pfd_service=efis-pfd.service
recorder_service=efis-recorder.service
acceptance_dropin=/home/efis/.config/systemd/user/efis-pfd.service.d/weather-async-acceptance.conf
evidence=/home/efis/.local/state/efis-next/release-pfd-weather-async-20260930
success=0

[[ ${EUID} -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
[[ $(hostname) == efis-pfd-01 ]] || { echo "wrong node" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ -S "$runtime_dir/bus" ]] || { echo "user systemd bus unavailable" >&2; exit 1; }
[[ $(readlink -f "$current") == "$rollback" ]] || { echo "unexpected active release" >&2; exit 1; }
[[ -d "$staged" && ! -L "$staged" ]] || { echo "prepared release missing" >&2; exit 1; }
[[ ! -e "$candidate" && ! -L "$candidate" ]] || { echo "candidate already exists" >&2; exit 1; }
[[ ! -e "${current}.new" && ! -L "${current}.new" ]] || { echo "stale atomic link" >&2; exit 1; }
[[ ! -e "$acceptance_dropin" ]] || { echo "stale acceptance drop-in" >&2; exit 1; }

user_systemctl() {
  runuser -u "$operator" -- env XDG_RUNTIME_DIR="$runtime_dir" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=$runtime_dir/bus" systemctl --user "$@"
}

validate_release() {
  local root=$1 role=$2
  python3 "$validator" --manifest "$root/release-manifest.json" \
    --release-root "$root" --expected-role "$role" \
    --expected-platform RPI5_AARCH64 --check-files
}

switch_release() {
  local target=$1
  ln -s "$target" "${current}.new"
  chown -h "$operator:$operator" "${current}.new"
  mv -Tf "${current}.new" "$current"
}

verify_runtime() {
  local expected=$1
  user_systemctl is-active --quiet "$recorder_service"
  user_systemctl is-active --quiet "$pfd_service"
  local pfd_pid recorder_pid
  pfd_pid=$(user_systemctl show "$pfd_service" -p MainPID --value)
  recorder_pid=$(user_systemctl show "$recorder_service" -p MainPID --value)
  [[ "$pfd_pid" =~ ^[1-9][0-9]*$ && "$recorder_pid" =~ ^[1-9][0-9]*$ ]]
  [[ $(readlink -f "/proc/$pfd_pid/exe") == "$expected/bin/efis-pfd-qt" ]]
  [[ $(readlink -f "/proc/$recorder_pid/exe") == "$expected/bin/efis-recorder" ]]
  validate_release "$expected" PFD
  validate_release "$expected" RECORDER
  ss -lunp | grep -Eq ':49002[[:space:]].*pid='"$pfd_pid"','
  [[ -s /home/efis/.local/state/efis-next/recorder.status ]]
}

recover() {
  local rc=$?
  rm -f "$acceptance_dropin"
  rm -f "${current}.new"
  user_systemctl daemon-reload || true
  if [[ $success -eq 0 ]]; then
    echo "activation failed (rc=$rc); restoring rollback release" >&2
    switch_release "$rollback" || true
    user_systemctl restart "$recorder_service" || true
    user_systemctl restart "$pfd_service" || true
  fi
  exit "$rc"
}
trap recover ERR

install -d -m 0750 -o "$operator" -g "$operator" "$evidence"
exec > >(tee -a "$evidence/activation.log") 2>&1
date -u +TIMESTAMP_UTC=%Y-%m-%dT%H:%M:%SZ
vcgencmd get_throttled | tee "$evidence/throttled-before.txt" | grep -qx 'throttled=0x0'
free_bytes=$(df --output=avail -B1 /home/efis | tail -n 1 | tr -d ' ')
[[ "$free_bytes" =~ ^[0-9]+$ && "$free_bytes" -ge 12884901888 ]]

install -D -m 0755 "$staged/tools/deployment/validate_release_manifest.py" "$validator"
cp -a "$staged" "$candidate"
chown -R root:root "$candidate"
chmod -R a-w "$candidate"
for role in PFD RECORDER; do validate_release "$candidate" "$role"; done
for role in PFD RECORDER; do validate_release "$rollback" "$role"; done

switch_release "$candidate"
candidate_started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
user_systemctl restart "$recorder_service"
user_systemctl restart "$pfd_service"
sleep 12
verify_runtime "$candidate"

switch_release "$rollback"
user_systemctl restart "$recorder_service"
user_systemctl restart "$pfd_service"
sleep 12
verify_runtime "$rollback"
echo ROLLBACK_DRILL=PASS | tee "$evidence/rollback-drill.txt"

switch_release "$candidate"
install -D -m 0644 -o "$operator" -g "$operator" /dev/stdin "$acceptance_dropin" <<'EOF'
[Service]
Environment=EFIS_WEATHER_SIMULATION_IO_DELAY_MS=400
EOF
user_systemctl daemon-reload
weather_started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
user_systemctl restart "$recorder_service"
user_systemctl restart "$pfd_service"
sleep 18
verify_runtime "$candidate"
journalctl _SYSTEMD_USER_UNIT="$pfd_service" --since "$weather_started" --no-pager \
  > "$evidence/weather-async-injected-journal.txt"
grep -q 'event=weather_simulation_worker_slow' "$evidence/weather-async-injected-journal.txt"
if grep -q 'event=callback_stall callback=weather_simulation' "$evidence/weather-async-injected-journal.txt"; then
  echo "weather callback stall detected" >&2
  exit 1
fi
echo WEATHER_ASYNC_INJECTED_IO=PASS | tee "$evidence/weather-async-result.txt"

rm -f "$acceptance_dropin"
user_systemctl daemon-reload
final_started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
user_systemctl restart "$pfd_service"
sleep 12
verify_runtime "$candidate"
journalctl _SYSTEMD_USER_UNIT="$pfd_service" --since "$final_started" --no-pager \
  > "$evidence/journal-final.txt"
if grep -q 'event=callback_stall callback=weather_simulation' "$evidence/journal-final.txt"; then
  echo "weather callback stall detected after removing diagnostic delay" >&2
  exit 1
fi

readlink -f "$current" | tee "$evidence/current-final.txt"
sha256sum "$candidate/bin/efis-pfd-qt" "$candidate/bin/efis-recorder" \
  > "$evidence/artifact-sha256.txt"
user_systemctl show "$pfd_service" "$recorder_service" \
  -p ActiveState -p SubState -p MainPID -p NRestarts -p Environment -p DropInPaths \
  > "$evidence/status-final.txt"
vcgencmd get_throttled | tee "$evidence/throttled-after.txt" | grep -qx 'throttled=0x0'
vcgencmd measure_temp | tee "$evidence/temperature-after.txt"

success=1
trap - ERR
chown -R "$operator:$operator" "$evidence"
echo PFD_WEATHER_ASYNC_ACTIVATION=PASS | tee "$evidence/result.txt"
echo NEXT_REQUIRED=REBOOT_ACCEPTANCE
