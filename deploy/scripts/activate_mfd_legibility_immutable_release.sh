#!/usr/bin/env bash
# Activate the staged MFD legibility release, prove rollback, then return to it.
# Must be run as root only in an explicitly authorized bench window.
set -Eeuo pipefail

operator=pi
candidate=/home/pi/efis-next-release-mfd-76afead-v1
rollback=/home/pi/efis-next-release-mfd-3432402-v1
current=/home/pi/efis-next-current
service=efis-next-mfd-direct.service
validator=/usr/local/lib/efis-next/validate_release_manifest.py
dropin_dir=/etc/systemd/system/efis-next-mfd-direct.service.d
dropin="$dropin_dir/release-v1.conf"
evidence=/home/pi/.local/state/efis-next/release-mfd-legibility-20261004
success=0

[[ ${EUID} -eq 0 ]] || { echo "run this script with sudo" >&2; exit 1; }
[[ $(hostname) == pi5 ]] || { echo "wrong node: expected pi5" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ $(readlink -f "$current") == "$rollback" ]] || {
  echo "unexpected active release; expected $rollback" >&2
  exit 1
}
[[ ! -e "${current}.new" && ! -L "${current}.new" ]] || {
  echo "stale atomic-switch path exists: ${current}.new" >&2
  exit 1
}
[[ -d "$candidate" && ! -L "$candidate" ]] || { echo "candidate missing" >&2; exit 1; }
[[ -d "$rollback" && ! -L "$rollback" ]] || { echo "rollback missing" >&2; exit 1; }
[[ ! -e "$evidence" && ! -L "$evidence" ]] || {
  echo "refusing to reuse activation evidence path: $evidence" >&2
  exit 1
}

mkdir -p "$evidence"
exec > >(tee -a "$evidence/activation.log") 2>&1

validate_release() {
  local root=$1
  python3 "$validator" \
    --manifest "$root/release-manifest.json" \
    --release-root "$root" \
    --expected-role MFD \
    --expected-platform RPI5_AARCH64 \
    --check-files
}

switch_release() {
  local target=$1
  ln -s "$target" "${current}.new"
  chown -h "$operator:$operator" "${current}.new"
  mv -Tf "${current}.new" "$current"
}

verify_conflicts_inactive() {
  local conflict
  for conflict in lightdm.service fuso-sv3d.service fuso-sv3d-sim.service efis-next-pfd-direct.service; do
    if systemctl is-active --quiet "$conflict"; then
      echo "conflicting display service is active: $conflict" >&2
      return 1
    fi
  done
}

verify_runtime() {
  local expected=$1 started=$2
  systemctl is-enabled --quiet "$service"
  systemctl is-active --quiet "$service"
  verify_conflicts_inactive
  local pid executable
  pid=$(systemctl show "$service" -p MainPID --value)
  [[ "$pid" =~ ^[1-9][0-9]*$ ]]
  executable=$(readlink -f "/proc/$pid/exe")
  [[ "$executable" == "$expected/bin/efis-pfd-qt" ]] || {
    echo "unexpected executable: $executable" >&2
    return 1
  }
  validate_release "$expected"
  ss -lunp | grep -Eq ':49002[[:space:]].*pid='"$pid"','
  ! journalctl -u "$service" --since "$started" --no-pager \
    | grep -Eiq 'failed to create display|drmModeGetResources failed|segmentation fault|core dumped'
}

restore_file() {
  local installed=$1 backup=$2
  if [[ -f "$backup" ]]; then
    install -m 0644 "$backup" "$installed"
  else
    rm -f "$installed"
  fi
}

recover() {
  local rc=$?
  if [[ $success -eq 0 ]]; then
    echo "activation failed (rc=$rc); restoring previous MFD runtime" >&2
    local recovery_link="${current}.recovery.$$"
    ln -s "$rollback" "$recovery_link" \
      && chown -h "$operator:$operator" "$recovery_link" \
      && mv -Tf "$recovery_link" "$current" \
      || true
    restore_file "$dropin" "$evidence/release-v1.before.conf"
    systemctl daemon-reload || true
    systemctl restart "$service" || true
  fi
  exit "$rc"
}
trap recover ERR

date -u +TIMESTAMP_UTC=%Y-%m-%dT%H:%M:%SZ
readlink -f "$current" > "$evidence/current-before.txt"
systemctl cat "$service" > "$evidence/service-before.txt"
systemctl show "$service" \
  -p ActiveState -p SubState -p MainPID -p NRestarts -p FragmentPath -p DropInPaths \
  > "$evidence/status-before.txt"
[[ ! -f "$dropin" ]] || cp -a "$dropin" "$evidence/release-v1.before.conf"
vcgencmd get_throttled | tee "$evidence/throttled-before.txt" | grep -qx 'throttled=0x0'
vcgencmd measure_temp | tee "$evidence/temperature-before.txt"
verify_conflicts_inactive

install -D -m 0755 "$candidate/tools/deployment/validate_release_manifest.py" "$validator"
install -d -m 0755 "$dropin_dir"
install -m 0644 \
  "$candidate/deploy/systemd/release-v1/efis-mfd-release-v1.override.conf" \
  "$dropin"

for release in "$candidate" "$rollback"; do
  chown -R root:root "$release"
  chmod -R a-w "$release"
  chmod 0644 "$release/release-manifest.json"
  validate_release "$release"
done

systemctl daemon-reload
systemd-analyze verify "$service" > "$evidence/systemd-analyze-verify.txt" 2>&1

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

readlink -f "$current" > "$evidence/current-candidate.txt"
systemctl show "$service" \
  -p ActiveState -p SubState -p MainPID -p NRestarts -p Environment -p DropInPaths \
  > "$evidence/status-candidate.txt"
journalctl -u "$service" --since "$candidate_started" --no-pager \
  > "$evidence/journal-candidate.txt"
sha256sum "$candidate/bin/efis-pfd-qt" "$candidate/release-manifest.json" \
  > "$evidence/artifact-sha256.txt"
vcgencmd get_throttled | tee "$evidence/throttled-after.txt" | grep -qx 'throttled=0x0'
vcgencmd measure_temp | tee "$evidence/temperature-after.txt"

success=1
trap - ERR
chown -R "$operator:$operator" "$evidence"
echo MFD_LEGIBILITY_IMMUTABLE_ACTIVATION=PASS | tee "$evidence/result.txt"
echo NEXT_REQUIRED=REBOOT_AND_VISUAL_ACCEPTANCE
