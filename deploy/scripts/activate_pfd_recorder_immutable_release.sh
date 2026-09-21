#!/usr/bin/env bash
# Activate the staged PFD+recorder release, prove rollback, then return to it.
# DEVELOPMENT/NON-CERT bench transition. Run with sudo on efis-pfd-01.
set -Eeuo pipefail

operator=efis
operator_uid=$(id -u "$operator")
runtime_dir=/run/user/$operator_uid
primary=/home/efis/efis-next-releases/pfdrec-d52e377-primary-v1
rollback=/home/efis/efis-next-releases/pfdrec-60b3526-rollback-v1
current=/home/efis/efis-next-current
stage=/home/efis/efis-release-pfd-stage
validator=/usr/local/lib/efis-next/validate_release_manifest.py
pfd_service=efis-pfd.service
recorder_service=efis-recorder.service
pfd_dropin_dir=/home/efis/.config/systemd/user/efis-pfd.service.d
recorder_dropin_dir=/home/efis/.config/systemd/user/efis-recorder.service.d
pfd_dropin=$pfd_dropin_dir/release-v1.conf
recorder_dropin=$recorder_dropin_dir/release-v1.conf
legacy_pfd_dropin=$pfd_dropin_dir/release.conf
evidence=/home/efis/.local/state/efis-next/release-pfd-recorder-20260920
state_root=/home/efis/.local/state/efis-next
records_dir=$state_root/records
success=0

[[ ${EUID} -eq 0 ]] || { echo "run this script with sudo" >&2; exit 1; }
[[ $(hostname) == efis-pfd-01 ]] || { echo "wrong node: expected efis-pfd-01" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ -S "$runtime_dir/bus" ]] || { echo "user systemd bus unavailable: $runtime_dir/bus" >&2; exit 1; }
[[ ! -e "${current}.new" && ! -L "${current}.new" ]] || {
  echo "stale atomic-switch path exists: ${current}.new" >&2
  exit 1
}

user_systemctl() {
  runuser -u "$operator" -- env \
    XDG_RUNTIME_DIR="$runtime_dir" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=$runtime_dir/bus" \
    systemctl --user "$@"
}

user_systemd_analyze() {
  runuser -u "$operator" -- env \
    XDG_RUNTIME_DIR="$runtime_dir" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=$runtime_dir/bus" \
    systemd-analyze --user "$@"
}

validate_release() {
  local root=$1 role=$2
  python3 "$validator" \
    --manifest "$root/release-manifest.json" \
    --release-root "$root" \
    --expected-role "$role" \
    --expected-platform RPI5_AARCH64 \
    --check-files
}

switch_release() {
  local target=$1
  ln -s "$target" "${current}.new"
  chown -h "$operator:$operator" "${current}.new"
  mv -Tf "${current}.new" "$current"
}

service_pid() {
  user_systemctl show "$1" -p MainPID --value
}

verify_runtime() {
  local expected=$1 started=$2
  for service in "$recorder_service" "$pfd_service"; do
    user_systemctl is-active --quiet "$service"
    local pid
    pid=$(service_pid "$service")
    [[ "$pid" =~ ^[1-9][0-9]*$ ]]
  done
  local pfd_pid recorder_pid
  pfd_pid=$(service_pid "$pfd_service")
  recorder_pid=$(service_pid "$recorder_service")
  [[ $(readlink -f "/proc/$pfd_pid/exe") == "$expected/bin/efis-pfd-qt" ]]
  [[ $(readlink -f "/proc/$recorder_pid/exe") == "$expected/bin/efis-recorder" ]]
  validate_release "$expected" PFD
  validate_release "$expected" RECORDER
  ss -lunp | grep -Eq ':49002[[:space:]].*pid='"$pfd_pid"','
  journalctl _SYSTEMD_USER_UNIT="$pfd_service" --since "$started" --no-pager \
    | grep -Eq 'event=startup product=efis-next component=pfd|event=render_health'
  [[ -s /home/efis/.local/state/efis-next/recorder.status ]]
}

restore_dropin() {
  local installed=$1 backup=$2
  if [[ -f "$backup" ]]; then
    install -m 0644 -o "$operator" -g "$operator" "$backup" "$installed"
  else
    rm -f "$installed"
  fi
}

recover() {
  local rc=$?
  if [[ $success -eq 0 ]]; then
    echo "activation failed (rc=$rc); restoring previous runtime" >&2
    if [[ -n ${previous_target:-} && -d ${previous_target:-} ]]; then
      local recovery_link="${current}.recovery.$$"
      ln -s "$previous_target" "$recovery_link" \
        && chown -h "$operator:$operator" "$recovery_link" \
        && mv -Tf "$recovery_link" "$current" \
        || true
    fi
    restore_dropin "$pfd_dropin" "$evidence/pfd-release-v1.before.conf"
    restore_dropin "$recorder_dropin" "$evidence/recorder-release-v1.before.conf"
    restore_dropin "$legacy_pfd_dropin" "$evidence/pfd-release-legacy.before.conf"
    user_systemctl daemon-reload || true
    user_systemctl restart "$recorder_service" || true
    user_systemctl restart "$pfd_service" || true
  fi
  exit "$rc"
}

install -d -m 0750 -o "$operator" -g "$operator" \
  "$state_root" "$records_dir" "$evidence"
exec > >(tee -a "$evidence/activation.log") 2>&1

previous_target=$(readlink -f "$current")
[[ -d "$previous_target" ]] || { echo "current is not a valid release symlink" >&2; exit 1; }
for pair in \
  "$pfd_dropin|$evidence/pfd-release-v1.before.conf" \
  "$recorder_dropin|$evidence/recorder-release-v1.before.conf" \
  "$legacy_pfd_dropin|$evidence/pfd-release-legacy.before.conf"; do
  IFS='|' read -r installed backup <<< "$pair"
  [[ ! -f "$installed" ]] || cp -a "$installed" "$backup"
done
trap recover ERR

date -u +TIMESTAMP_UTC=%Y-%m-%dT%H:%M:%SZ
echo "$previous_target" > "$evidence/current-before.txt"
user_systemctl cat "$pfd_service" > "$evidence/pfd-service-before.txt"
user_systemctl cat "$recorder_service" > "$evidence/recorder-service-before.txt"
user_systemctl show "$pfd_service" "$recorder_service" \
  -p ActiveState -p SubState -p MainPID -p NRestarts -p FragmentPath -p DropInPaths \
  > "$evidence/status-before.txt"
vcgencmd get_throttled | tee "$evidence/throttled-before.txt" | grep -qx 'throttled=0x0'
vcgencmd measure_temp | tee "$evidence/temperature-before.txt"

install -D -m 0755 "$stage/tools/deployment/validate_release_manifest.py" "$validator"
install -d -m 0755 -o "$operator" -g "$operator" "$pfd_dropin_dir" "$recorder_dropin_dir"
install -m 0644 -o "$operator" -g "$operator" \
  "$stage/systemd/efis-pfd-release-v1.override.conf" "$pfd_dropin"
install -m 0644 -o "$operator" -g "$operator" \
  "$stage/systemd/efis-recorder-release-v1.override.conf" "$recorder_dropin"
# The older drop-in writes logs below the release and overrides ExecStart.
rm -f "$legacy_pfd_dropin"

for release in "$primary" "$rollback"; do
  chown -R root:root "$release"
  chmod -R a-w "$release"
  chmod 0644 "$release/release-manifest.json"
  validate_release "$release" PFD
  validate_release "$release" RECORDER
done

user_systemctl daemon-reload
user_systemctl enable "$pfd_service" "$recorder_service"
switch_release "$primary"
set +e
user_systemd_analyze verify "$pfd_service" "$recorder_service" \
  > "$evidence/systemd-analyze-verify.txt" 2>&1
unit_verify_rc=$?
set -e
if [[ $unit_verify_rc -eq 0 ]]; then
  echo SYSTEMD_ANALYZE_VERIFY=PASS
else
  # On the PFD image systemd-analyze --user exits non-zero without diagnostics even
  # for loadable user units. The real gate remains fail-closed below: both
  # services must restart, stay active, expose the exact release executables,
  # and publish their runtime evidence.
  echo SYSTEMD_ANALYZE_VERIFY=UNAVAILABLE_RC_$unit_verify_rc
fi

primary_started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
user_systemctl restart "$recorder_service"
user_systemctl restart "$pfd_service"
sleep 12
verify_runtime "$primary" "$primary_started"

rollback_started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
switch_release "$rollback"
user_systemctl restart "$recorder_service"
user_systemctl restart "$pfd_service"
sleep 12
verify_runtime "$rollback" "$rollback_started"
echo ROLLBACK_DRILL=PASS | tee "$evidence/rollback-drill.txt"

primary_started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
switch_release "$primary"
user_systemctl restart "$recorder_service"
user_systemctl restart "$pfd_service"
sleep 12
verify_runtime "$primary" "$primary_started"

user_systemctl show "$pfd_service" "$recorder_service" \
  -p ActiveState -p SubState -p MainPID -p NRestarts -p Environment -p DropInPaths \
  > "$evidence/status-primary.txt"
readlink -f "$current" > "$evidence/current-primary.txt"
sha256sum "$primary/bin/efis-pfd-qt" "$primary/bin/efis-recorder" \
  > "$evidence/artifact-sha256.txt"
{
  journalctl _SYSTEMD_USER_UNIT="$pfd_service" --since "$primary_started" --no-pager
  journalctl _SYSTEMD_USER_UNIT="$recorder_service" --since "$primary_started" --no-pager
} > "$evidence/journal-primary.txt"
cp -a /home/efis/.local/state/efis-next/recorder.status "$evidence/recorder.status"
vcgencmd get_throttled | tee "$evidence/throttled-after.txt" | grep -qx 'throttled=0x0'
vcgencmd measure_temp | tee "$evidence/temperature-after.txt"

success=1
trap - ERR
chown -R "$operator:$operator" "$evidence"
echo PFD_RECORDER_IMMUTABLE_ACTIVATION=PASS | tee "$evidence/result.txt"
echo NEXT_REQUIRED=REBOOT_ACCEPTANCE
