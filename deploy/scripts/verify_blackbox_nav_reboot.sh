#!/usr/bin/env bash
# Read-only post-reboot acceptance for the manifest-covered bbxnav policy.
set -Eeuo pipefail

release=/opt/efis-blackbox-releases/blackbox-46e56a9-nav-supervision-v1
current=/opt/efis-blackbox-current
validator=/usr/local/lib/efis-next/validate_release_manifest.py
broker_service=efis-blackbox-broker.service
nav_service=bbxnav.service
nav_dropin=/etc/systemd/system/${nav_service}.d/20-fail-closed.conf
expected_nav=$current/share/systemd/bbxnav-fail-closed.override.conf
evidence=/var/lib/efis-next/evidence/blackbox-nav-release-20260930

[[ ${EUID} -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
[[ $(hostname) == efisblackbox ]] || { echo "wrong node" >&2; exit 1; }
[[ $(uname -m) == aarch64 ]] || { echo "wrong architecture" >&2; exit 1; }
[[ $(readlink -f "$current") == "$release" ]] || { echo "unexpected active release" >&2; exit 1; }
[[ -L "$nav_dropin" && $(readlink "$nav_dropin") == "$expected_nav" ]] || {
  echo "NAV drop-in is not bound to the active release" >&2
  exit 1
}

python3 "$validator" --manifest "$release/release-manifest.json" \
  --release-root "$release" --expected-role BLACKBOX \
  --expected-platform RPI5_AARCH64 --check-files
systemctl is-enabled --quiet "$broker_service"
systemctl is-active --quiet "$broker_service"
broker_pid=$(systemctl show "$broker_service" -p MainPID --value)
[[ "$broker_pid" =~ ^[1-9][0-9]*$ ]]
[[ $(readlink -f "/proc/$broker_pid/exe") == "$release/bin/efis-blackbox-broker" ]]
[[ $(systemctl show "$nav_service" -p ExecMainStatus --value) == 2 ]]
[[ $(systemctl show "$nav_service" -p NRestarts --value) == 0 ]]
before=$(systemctl show "$nav_service" -p NRestarts --value)
sleep 30
after=$(systemctl show "$nav_service" -p NRestarts --value)
[[ "$before" == 0 && "$after" == 0 ]]

mkdir -p "$evidence"
{
  date -u +TIMESTAMP_UTC=%Y-%m-%dT%H:%M:%SZ
  echo BOOT_ID=$(cat /proc/sys/kernel/random/boot_id)
  echo CURRENT=$(readlink -f "$current")
  echo NAV_DROPIN=$(readlink "$nav_dropin")
  echo BROKER_PID=$broker_pid
  systemctl show "$broker_service" "$nav_service" \
    -p ActiveState -p SubState -p Result -p ExecMainStatus -p MainPID -p NRestarts -p DropInPaths
  vcgencmd get_throttled
  vcgencmd measure_temp
  sha256sum "$release/bin/efis-blackbox-broker" \
    "$release/share/systemd/bbxnav-fail-closed.override.conf"
  echo NAV_RESTARTS_BEFORE=$before
  echo NAV_RESTARTS_AFTER=$after
  echo BLACKBOX_NAV_REBOOT_ACCEPTANCE=PASS
} | tee "$evidence/post-reboot-acceptance.txt"
grep -qx 'throttled=0x0' "$evidence/post-reboot-acceptance.txt"
