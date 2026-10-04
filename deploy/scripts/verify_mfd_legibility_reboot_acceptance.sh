#!/usr/bin/env bash
# Read-only post-reboot release acceptance for the MFD legibility candidate.
# Visual/touch acceptance remains a separate operator gate.
set -Eeuo pipefail

operator=pi
release=/home/pi/efis-next-release-mfd-76afead-v1
current=/home/pi/efis-next-current
service=efis-next-mfd-direct.service
validator=/usr/local/lib/efis-next/validate_release_manifest.py
evidence=/home/pi/.local/state/efis-next/release-mfd-legibility-20261004

[[ ${EUID} -eq 0 ]] || { echo "run this script with sudo" >&2; exit 1; }
[[ $(hostname) == pi5 ]] || { echo "wrong node: expected pi5" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ $(readlink -f "$current") == "$release" ]] || { echo "unexpected active release" >&2; exit 1; }

python3 "$validator" \
  --manifest "$release/release-manifest.json" \
  --release-root "$release" \
  --expected-role MFD \
  --expected-platform RPI5_AARCH64 \
  --check-files
systemctl is-enabled --quiet "$service"
systemctl is-active --quiet "$service"

for conflict in lightdm.service fuso-sv3d.service fuso-sv3d-sim.service efis-next-pfd-direct.service; do
  ! systemctl is-active --quiet "$conflict" || {
    echo "conflicting display service is active: $conflict" >&2
    exit 1
  }
done

pid=$(systemctl show "$service" -p MainPID --value)
[[ "$pid" =~ ^[1-9][0-9]*$ ]]
[[ $(readlink -f "/proc/$pid/exe") == "$release/bin/efis-pfd-qt" ]]
ss -lunp | grep -Eq ':49002[[:space:]].*pid='"$pid"','

mkdir -p "$evidence"
{
  date -u +TIMESTAMP_UTC=%Y-%m-%dT%H:%M:%SZ
  echo BOOT_ID=$(cat /proc/sys/kernel/random/boot_id)
  echo CURRENT=$(readlink -f "$current")
  echo MFD_PID=$pid
  systemctl show "$service" \
    -p ActiveState -p SubState -p MainPID -p NRestarts -p ExecMainStatus -p DropInPaths
  vcgencmd get_throttled
  vcgencmd measure_temp
  sha256sum "$release/bin/efis-pfd-qt" "$release/release-manifest.json"
  echo MFD_LEGIBILITY_REBOOT_ACCEPTANCE=PASS
  echo VISUAL_TOUCH_ACCEPTANCE=NOT_RUN
  echo FLIGHT_APPROVAL=false
} | tee "$evidence/post-reboot-acceptance.txt"

grep -qx 'throttled=0x0' "$evidence/post-reboot-acceptance.txt"
! journalctl -u "$service" -b --no-pager \
  | grep -Eiq 'failed to create display|drmModeGetResources failed|segmentation fault|core dumped'
chown -R "$operator:$operator" "$evidence"
