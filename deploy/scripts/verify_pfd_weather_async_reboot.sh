#!/usr/bin/env bash
# Read-only post-reboot acceptance for the PFD asynchronous weather release.
set -Eeuo pipefail

operator=efis
operator_uid=$(id -u "$operator")
runtime_dir=/run/user/$operator_uid
release=/home/efis/efis-next-releases/pfdrec-3c7bb4d-weather-async-v1
current=/home/efis/efis-next-current
validator=/usr/local/lib/efis-next/validate_release_manifest.py
evidence=/home/efis/.local/state/efis-next/release-pfd-weather-async-20260930

[[ ${EUID} -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
[[ $(hostname) == efis-pfd-01 ]] || { echo "wrong node" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ -S "$runtime_dir/bus" ]] || { echo "user systemd bus unavailable" >&2; exit 1; }
[[ $(readlink -f "$current") == "$release" ]] || { echo "unexpected active release" >&2; exit 1; }

user_systemctl() {
  runuser -u "$operator" -- env XDG_RUNTIME_DIR="$runtime_dir" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=$runtime_dir/bus" systemctl --user "$@"
}

for role in PFD RECORDER; do
  python3 "$validator" --manifest "$release/release-manifest.json" \
    --release-root "$release" --expected-role "$role" \
    --expected-platform RPI5_AARCH64 --check-files
done
for service in efis-recorder.service efis-pfd.service; do
  user_systemctl is-enabled --quiet "$service"
  user_systemctl is-active --quiet "$service"
done

pfd_pid=$(user_systemctl show efis-pfd.service -p MainPID --value)
recorder_pid=$(user_systemctl show efis-recorder.service -p MainPID --value)
[[ $(readlink -f "/proc/$pfd_pid/exe") == "$release/bin/efis-pfd-qt" ]]
[[ $(readlink -f "/proc/$recorder_pid/exe") == "$release/bin/efis-recorder" ]]
ss -lunp | grep -Eq ':49002[[:space:]].*pid='"$pfd_pid"','
status=/home/efis/.local/state/efis-next/recorder.status
before=$(stat -c %Y "$status")
started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
sleep 30
after=$(stat -c %Y "$status")
[[ $after -gt $before ]]

mkdir -p "$evidence"
journalctl _SYSTEMD_USER_UNIT=efis-pfd.service --since "$started" --no-pager \
  > "$evidence/post-reboot-weather-journal.txt"
if grep -q 'event=callback_stall callback=weather_simulation' "$evidence/post-reboot-weather-journal.txt"; then
  echo "weather callback stall detected after reboot" >&2
  exit 1
fi
{
  date -u +TIMESTAMP_UTC=%Y-%m-%dT%H:%M:%SZ
  echo BOOT_ID=$(cat /proc/sys/kernel/random/boot_id)
  echo CURRENT=$(readlink -f "$current")
  echo PFD_PID=$pfd_pid
  echo RECORDER_PID=$recorder_pid
  user_systemctl show efis-pfd.service efis-recorder.service \
    -p ActiveState -p SubState -p MainPID -p NRestarts -p DropInPaths
  vcgencmd get_throttled
  vcgencmd measure_temp
  sha256sum "$release/bin/efis-pfd-qt" "$release/bin/efis-recorder"
  echo RECORDER_STATUS_MTIME_BEFORE=$before
  echo RECORDER_STATUS_MTIME_AFTER=$after
  echo PFD_WEATHER_ASYNC_REBOOT_ACCEPTANCE=PASS
} | tee "$evidence/post-reboot-acceptance.txt"
grep -qx 'throttled=0x0' "$evidence/post-reboot-acceptance.txt"
chown -R "$operator:$operator" "$evidence"
