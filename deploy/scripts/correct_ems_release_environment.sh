#!/usr/bin/env bash
# Restore launcher-provided EMS kiosk/render/EDP variables when systemd starts
# the immutable binary directly.
set -Eeuo pipefail

service=efis-ems.service
dropin=/etc/systemd/system/efis-ems.service.d/release-v1.conf
candidate=/home/pi/efis-release-ems-stage/systemd/efis-ems-release-v1.override.conf
evidence=/home/pi/.local/state/efis-next/release-migration-ems-20260920
backup="$evidence/release-v1.before-environment-fix.conf"
success=0

[[ ${EUID} -eq 0 ]] || { echo "run this script with sudo" >&2; exit 1; }
[[ $(hostname) == efis ]] || { echo "wrong node: expected efis" >&2; exit 1; }
[[ -f "$candidate" ]] || { echo "candidate drop-in missing" >&2; exit 1; }

mkdir -p "$evidence"
exec > >(tee -a "$evidence/release-environment-correction.log") 2>&1
cp -a "$dropin" "$backup"

recover() {
  local rc=$?
  if [[ $success -eq 0 ]]; then
    echo "release environment correction failed (rc=$rc); restoring previous drop-in" >&2
    install -m 0644 "$backup" "$dropin"
    systemctl daemon-reload || true
    systemctl restart "$service" || true
  fi
  exit "$rc"
}
trap recover ERR

install -m 0644 "$candidate" "$dropin"
systemctl daemon-reload
systemd-analyze verify "$service"
started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
systemctl restart "$service"
sleep 12
systemctl is-active --quiet "$service"
pid=$(systemctl show "$service" -p MainPID --value)
[[ $(readlink -f "/proc/$pid/exe") == /home/pi/efis-next-releases/ems-7655799-v1/bin/efis-ems-qt ]]

environment=$(tr '\0' '\n' < "/proc/$pid/environ")
grep -qx 'EFIS_KIOSK=1' <<< "$environment"
grep -qx 'QSG_RENDER_LOOP=threaded' <<< "$environment"
grep -qx 'EFIS_EDP_BIND=10.77.10.12' <<< "$environment"
grep -qx 'EFIS_EDP_PORT=49003' <<< "$environment"
grep -qx 'EFIS_EDP_SOURCE_NODE=47' <<< "$environment"
ss -lun | grep -Eq '10\.77\.10\.12:49003[[:space:]]'
# `state=SIM` is the provenance of the engine payload, not a lost link. The
# live-link evidence is engine_state=LIVE, accepted datagrams and BLACKBOX 47.
journalctl -u "$service" --since "$started" --no-pager \
  | grep -Eq 'event=ems_edp_link_transition .*engine_state= LIVE .*accepted= [1-9][0-9]* .*node= BLACKBOX 47'
journalctl -u "$service" --since "$started" --no-pager \
  | grep -Eq 'event=render_health node=ems'
if journalctl -u "$service" --since "$started" --no-pager \
    | grep -Eq 'engine_runtime_(save_failed|rejected)'; then
  echo "runtime persistence error after environment correction" >&2
  exit 1
fi

systemctl show "$service" -p ActiveState -p SubState -p MainPID -p NRestarts -p Environment \
  > "$evidence/status-environment-corrected.txt"
journalctl -u "$service" --since "$started" --no-pager \
  > "$evidence/journal-environment-corrected.txt"
success=1
trap - ERR
echo "EMS_RELEASE_ENVIRONMENT_CORRECTION=PASS" \
  | tee "$evidence/release-environment-result.txt"
