#!/usr/bin/env bash
# Activate the EMS HMI/CAS candidate, prove rollback, and return to candidate.
# This is a bench-only DEVELOPMENT/NON-CERT transition.
set -Eeuo pipefail

candidate=/home/pi/efis-next-releases/ems-62ba90b-hmi-cas-v1
rollback=/home/pi/efis-next-releases/ems-7655799-v1
current=/home/pi/efis-next-current
service=efis-ems.service
validator=/usr/local/lib/efis-next/validate_release_manifest.py
evidence=/home/pi/.local/state/efis-next/release-ems-hmi-cas-20260920
expected_commit=62ba90bcc53799303af2fa1de5b8ec3b1b5fb35f
expected_hash=4ebab1d5a7a10387987be49eb4d36554dccfb67e5ccd08d6e461b484b96073f7
success=0

[[ ${EUID} -eq 0 ]] || { echo "run this script with sudo" >&2; exit 1; }
[[ $(hostname) == efis ]] || { echo "wrong node: expected efis" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ $(readlink -f "$current") == "$rollback" ]] || {
  echo "unexpected active release before transition: $(readlink -f "$current")" >&2
  exit 1
}
[[ ! -e "${current}.new" && ! -L "${current}.new" ]] || {
  echo "stale atomic-switch path exists: ${current}.new" >&2
  exit 1
}

mkdir -p "$evidence"
exec > >(tee -a "$evidence/activation.log") 2>&1

validate_release() {
  local root=$1
  python3 "$validator" \
    --manifest "$root/release-manifest.json" \
    --release-root "$root" \
    --expected-role EMS \
    --expected-platform RPI4_AARCH64 \
    --check-files
}

switch_release() {
  local target=$1
  ln -s "$target" "${current}.new"
  mv -Tf "${current}.new" "$current"
}

verify_runtime() {
  local expected=$1
  local started=$2
  systemctl is-active --quiet "$service"
  local pid executable environment
  pid=$(systemctl show "$service" -p MainPID --value)
  [[ "$pid" =~ ^[1-9][0-9]*$ ]]
  executable=$(readlink -f "/proc/$pid/exe")
  [[ "$executable" == "$expected/bin/efis-ems-qt" ]]
  validate_release "$expected"
  environment=$(tr '\0' '\n' < "/proc/$pid/environ")
  grep -qx 'EFIS_KIOSK=1' <<< "$environment"
  grep -qx 'QSG_RENDER_LOOP=threaded' <<< "$environment"
  grep -qx 'EFIS_EDP_BIND=10.77.10.12' <<< "$environment"
  grep -qx 'EFIS_EDP_PORT=49003' <<< "$environment"
  grep -qx 'EFIS_EDP_SOURCE_NODE=47' <<< "$environment"
  ss -lun | grep -Eq '10\.77\.10\.12:49003[[:space:]]'
  journalctl -u "$service" --since "$started" --no-pager \
    | grep -Eq 'event=ems_edp_link_transition .*engine_state= LIVE .*accepted= [1-9][0-9]* .*node= BLACKBOX 47'
  journalctl -u "$service" --since "$started" --no-pager \
    | grep -Eq 'event=startup product=efis-next component=ems'
}

recover() {
  local rc=$?
  if [[ $success -eq 0 ]]; then
    echo "activation failed (rc=$rc); restoring $rollback" >&2
    local recovery_link="${current}.recovery.$$"
    ln -s "$rollback" "$recovery_link" \
      && mv -Tf "$recovery_link" "$current" \
      || true
    systemctl restart "$service" || true
  fi
  exit "$rc"
}
trap recover ERR

date -u +TIMESTAMP_UTC=%Y-%m-%dT%H:%M:%SZ
systemctl cat "$service" > "$evidence/service-before.txt"
systemctl show "$service" -p ActiveState -p SubState -p MainPID -p NRestarts \
  > "$evidence/status-before.txt"
readlink -f "$current" > "$evidence/current-before.txt"
throttled=$(vcgencmd get_throttled)
echo "$throttled" | tee "$evidence/throttled-before.txt"
[[ "$throttled" == 'throttled=0x0' ]] || {
  echo "unsafe power/thermal history before activation: $throttled" >&2
  exit 1
}
vcgencmd measure_temp | tee "$evidence/temperature-before.txt"

validate_release "$rollback"
validate_release "$candidate"
grep -q "\"source_commit\": \"$expected_commit\"" "$candidate/release-manifest.json"
echo "$expected_hash  $candidate/bin/efis-ems-qt" | sha256sum -c -

chown -R root:root "$candidate"
chmod -R a-w "$candidate"
chmod 0644 "$candidate/release-manifest.json"
validate_release "$candidate"

candidate_started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
switch_release "$candidate"
systemctl restart "$service"
sleep 12
verify_runtime "$candidate" "$candidate_started"

rollback_started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
switch_release "$rollback"
systemctl restart "$service"
sleep 12
verify_runtime "$rollback" "$rollback_started"
echo ROLLBACK_DRILL=PASS | tee "$evidence/rollback-drill.txt"

candidate_started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
switch_release "$candidate"
systemctl restart "$service"
sleep 12
verify_runtime "$candidate" "$candidate_started"

pid=$(systemctl show "$service" -p MainPID --value)
captures=/home/pi/.local/state/efis-next/ems/captures
before=$(find "$captures" -maxdepth 1 -type f -name 'ems-*.png' | wc -l)
kill -USR1 "$pid"
sleep 5
after=$(find "$captures" -maxdepth 1 -type f -name 'ems-*.png' | wc -l)
[[ $after -gt $before ]]
find "$captures" -maxdepth 1 -type f -name 'ems-*.png' -printf '%T@ %p\n' \
  | sort -n | tail -n 1 | cut -d' ' -f2- > "$evidence/capture-path.txt"
capture=$(cat "$evidence/capture-path.txt")
file "$capture" | tee "$evidence/capture-file.txt"
sha256sum "$capture" > "$evidence/capture-sha256.txt"

systemctl show "$service" -p ActiveState -p SubState -p MainPID -p NRestarts -p Environment \
  > "$evidence/status-candidate.txt"
readlink -f "$current" > "$evidence/current-candidate.txt"
sha256sum "$candidate/bin/efis-ems-qt" > "$evidence/artifact-sha256.txt"
journalctl -u "$service" --since "$candidate_started" --no-pager \
  > "$evidence/journal-candidate.txt"
vcgencmd get_throttled | tee "$evidence/throttled-after.txt"
vcgencmd measure_temp | tee "$evidence/temperature-after.txt"

success=1
trap - ERR
echo EMS_HMI_CAS_ACTIVATION=PASS | tee "$evidence/result.txt"
echo NEXT_REQUIRED=REBOOT_ACCEPTANCE
