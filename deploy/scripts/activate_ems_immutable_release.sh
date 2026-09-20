#!/usr/bin/env bash
# Activate the staged EMS release, exercise rollback, then return to primary.
# Must be run as root on the EMS node after stage_ems_immutable_releases.sh.
set -Eeuo pipefail

primary=/home/pi/efis-next-releases/ems-7655799-v1
rollback=/home/pi/efis-next-releases/ems-67aee2e-rollback-v1
current=/home/pi/efis-next-current
stage=/home/pi/efis-release-ems-stage
service=efis-ems.service
validator=/usr/local/lib/efis-next/validate_release_manifest.py
dropin_dir=/etc/systemd/system/efis-ems.service.d
dropin="$dropin_dir/release-v1.conf"
evidence=/home/pi/.local/state/efis-next/release-migration-ems-20260920
success=0

[[ ${EUID} -eq 0 ]] || { echo "run this script with sudo" >&2; exit 1; }
[[ $(hostname) == efis ]] || { echo "wrong node: expected efis" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ ! -e "$current" && ! -L "$current" ]] || {
  echo "refusing unexpected pre-existing current path: $current" >&2
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
  systemctl is-active --quiet "$service"
  local pid
  pid=$(systemctl show "$service" -p MainPID --value)
  [[ "$pid" =~ ^[1-9][0-9]*$ ]]
  local executable
  executable=$(readlink -f "/proc/$pid/exe")
  [[ "$executable" == "$expected/bin/efis-ems-qt" ]] || {
    echo "unexpected executable: $executable" >&2
    return 1
  }
  validate_release "$expected"
}

recover_legacy() {
  local rc=$?
  if [[ $success -eq 0 ]]; then
    echo "activation failed (rc=$rc); restoring legacy service" >&2
    rm -f "$dropin"
    rm -f "$current" "${current}.new"
    systemctl daemon-reload || true
    systemctl restart "$service" || true
  fi
  exit "$rc"
}
trap recover_legacy ERR

date -u +TIMESTAMP_UTC=%Y-%m-%dT%H:%M:%SZ
systemctl cat "$service" > "$evidence/service-before.txt"
systemctl show "$service" -p ActiveState -p SubState -p MainPID -p NRestarts \
  > "$evidence/status-before.txt"
sha256sum "$primary/bin/efis-ems-qt" "$rollback/bin/efis-ems-qt" \
  > "$evidence/artifact-sha256.txt"

install -D -m 0755 "$stage/tools/deployment/validate_release_manifest.py" "$validator"
install -d -m 0755 "$dropin_dir"
install -m 0644 "$stage/systemd/efis-ems-release-v1.override.conf" "$dropin"

for release in "$primary" "$rollback"; do
  chown -R root:root "$release"
  chmod -R a-w "$release"
  chmod 0644 "$release/release-manifest.json"
  validate_release "$release"
done

switch_release "$primary"
systemctl daemon-reload
systemd-analyze verify "$service"
systemctl restart "$service"
sleep 8
verify_runtime "$primary"

switch_release "$rollback"
systemctl restart "$service"
sleep 8
verify_runtime "$rollback"
echo "ROLLBACK_DRILL=PASS" | tee "$evidence/rollback-drill.txt"

switch_release "$primary"
systemctl restart "$service"
sleep 8
verify_runtime "$primary"
systemctl show "$service" -p ActiveState -p SubState -p MainPID -p NRestarts \
  > "$evidence/status-primary.txt"
readlink -f "$current" > "$evidence/current-target.txt"
systemctl cat "$service" > "$evidence/service-after.txt"

success=1
trap - ERR
echo "EMS_IMMUTABLE_ACTIVATION=PASS" | tee "$evidence/result.txt"
echo "NEXT_REQUIRED=REBOOT_ACCEPTANCE"
