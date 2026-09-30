#!/usr/bin/env bash
# Activate the prepared Blackbox release, bind the bbxnav supervision drop-in
# to its manifest-covered asset, and prove rollback. DEVELOPMENT/NON-CERT only.
set -Eeuo pipefail

release_id=blackbox-46e56a9-nav-supervision-v1
candidate=/opt/efis-blackbox-releases/$release_id
staged=/home/pi/efis-release-blackbox-nav-stage/release-$release_id
rollback=/opt/efis-blackbox-releases/blackbox-7655799-v1
current=/opt/efis-blackbox-current
validator=/usr/local/lib/efis-next/validate_release_manifest.py
broker_service=efis-blackbox-broker.service
nav_service=bbxnav.service
nav_dir=/etc/systemd/system/${nav_service}.d
nav_dropin=$nav_dir/20-fail-closed.conf
candidate_nav=$current/share/systemd/bbxnav-fail-closed.override.conf
evidence=/var/lib/efis-next/evidence/blackbox-nav-release-20260930
backup=$evidence/pre-activation
success=0
original_current=
original_dropin_kind=absent

[[ ${EUID} -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
[[ $(hostname) == efisblackbox ]] || { echo "wrong node" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ $(readlink -f "$current") == "$rollback" ]] || { echo "unexpected active release" >&2; exit 1; }
[[ -d "$staged" && ! -L "$staged" ]] || { echo "prepared release missing" >&2; exit 1; }
[[ ! -e "$candidate" && ! -L "$candidate" ]] || { echo "candidate already exists" >&2; exit 1; }
[[ ! -e "${current}.new" && ! -L "${current}.new" ]] || { echo "stale atomic link" >&2; exit 1; }

validate_release() {
  local root=$1
  python3 "$validator" --manifest "$root/release-manifest.json" \
    --release-root "$root" --expected-role BLACKBOX \
    --expected-platform RPI5_AARCH64 --check-files
}

switch_release() {
  local target=$1
  ln -s "$target" "${current}.new"
  mv -Tf "${current}.new" "$current"
}

link_candidate_nav() {
  install -d -m 0755 "$nav_dir"
  ln -s "$candidate_nav" "${nav_dropin}.new"
  mv -Tf "${nav_dropin}.new" "$nav_dropin"
  [[ $(readlink "$nav_dropin") == "$candidate_nav" ]]
}

restore_original_nav() {
  rm -f "${nav_dropin}.new" "$nav_dropin"
  if [[ $original_dropin_kind == file ]]; then
    install -m 0644 "$backup/20-fail-closed.conf" "$nav_dropin"
  elif [[ $original_dropin_kind == symlink ]]; then
    ln -s "$(cat "$backup/20-fail-closed.symlink-target")" "$nav_dropin"
  fi
}

verify_broker() {
  local expected=$1
  systemctl is-active --quiet "$broker_service"
  local pid
  pid=$(systemctl show "$broker_service" -p MainPID --value)
  [[ "$pid" =~ ^[1-9][0-9]*$ ]]
  [[ $(readlink -f "/proc/$pid/exe") == "$expected/bin/efis-blackbox-broker" ]]
  validate_release "$expected"
}

recover() {
  local rc=$?
  rm -f "${current}.new" "${nav_dropin}.new"
  if [[ $success -eq 0 && -n $original_current ]]; then
    echo "activation failed (rc=$rc); restoring previous release and NAV policy" >&2
    switch_release "$original_current" || true
    restore_original_nav || true
    systemctl daemon-reload || true
    systemctl restart "$broker_service" || true
  fi
  exit "$rc"
}
trap recover ERR

install -d -m 0750 "$backup"
exec > >(tee -a "$evidence/activation.log") 2>&1
date -u +TIMESTAMP_UTC=%Y-%m-%dT%H:%M:%SZ
vcgencmd get_throttled | tee "$evidence/throttled-before.txt" | grep -qx 'throttled=0x0'
original_current=$(readlink -f "$current")
printf '%s\n' "$original_current" > "$backup/current-target.txt"
if [[ -L "$nav_dropin" ]]; then
  original_dropin_kind=symlink
  readlink "$nav_dropin" > "$backup/20-fail-closed.symlink-target"
elif [[ -f "$nav_dropin" ]]; then
  original_dropin_kind=file
  cp -a "$nav_dropin" "$backup/20-fail-closed.conf"
elif [[ -e "$nav_dropin" ]]; then
  echo "unsupported existing NAV drop-in type" >&2
  exit 1
fi
printf '%s\n' "$original_dropin_kind" > "$backup/dropin-kind.txt"

install -D -m 0755 "$staged/tools/deployment/validate_release_manifest.py" "$validator"
cp -a "$staged" "$candidate"
chown -R root:root "$candidate"
chmod -R a-w "$candidate"
validate_release "$candidate"
validate_release "$rollback"

switch_release "$candidate"
link_candidate_nav
systemctl daemon-reload
systemd-analyze verify "$broker_service" "$nav_service"
systemctl restart "$broker_service"
systemctl reset-failed "$nav_service" || true
systemctl start "$nav_service" || true
sleep 15
verify_broker "$candidate"
nav_status=$(systemctl show "$nav_service" -p ExecMainStatus --value)
nav_restarts=$(systemctl show "$nav_service" -p NRestarts --value)
[[ "$nav_status" == 2 && "$nav_restarts" == 0 ]]
sleep 15
[[ $(systemctl show "$nav_service" -p NRestarts --value) == 0 ]]

switch_release "$rollback"
restore_original_nav
systemctl daemon-reload
systemctl restart "$broker_service"
sleep 12
verify_broker "$rollback"
echo ROLLBACK_DRILL=PASS | tee "$evidence/rollback-drill.txt"

switch_release "$candidate"
link_candidate_nav
systemctl daemon-reload
systemctl restart "$broker_service"
systemctl reset-failed "$nav_service" || true
systemctl start "$nav_service" || true
sleep 15
verify_broker "$candidate"
[[ $(systemctl show "$nav_service" -p ExecMainStatus --value) == 2 ]]
[[ $(systemctl show "$nav_service" -p NRestarts --value) == 0 ]]

readlink -f "$current" | tee "$evidence/current-final.txt"
readlink "$nav_dropin" | tee "$evidence/nav-dropin-final.txt"
sha256sum "$candidate/bin/efis-blackbox-broker" \
  "$candidate/share/systemd/bbxnav-fail-closed.override.conf" \
  > "$evidence/artifact-sha256.txt"
systemctl show "$broker_service" "$nav_service" \
  -p ActiveState -p SubState -p Result -p ExecMainStatus -p MainPID -p NRestarts -p DropInPaths \
  > "$evidence/status-final.txt"
vcgencmd get_throttled | tee "$evidence/throttled-after.txt" | grep -qx 'throttled=0x0'
vcgencmd measure_temp | tee "$evidence/temperature-after.txt"

success=1
trap - ERR
echo BLACKBOX_NAV_IMMUTABLE_ACTIVATION=PASS | tee "$evidence/result.txt"
echo NEXT_REQUIRED=REBOOT_ACCEPTANCE
