#!/usr/bin/env bash
# Correct runtime paths after the immutable EMS activation. Preserves all
# legacy state and refuses to hide persistence failures.
set -Eeuo pipefail

service=efis-ems.service
dropin=/etc/systemd/system/efis-ems.service.d/release-v1.conf
candidate=/home/pi/efis-release-ems-stage/systemd/efis-ems-release-v1.override.conf
runtime=/home/pi/.local/state/efis-next/ems
evidence=/home/pi/.local/state/efis-next/release-migration-ems-20260920
backup="$evidence/release-v1.before-runtime-fix.conf"
success=0

[[ ${EUID} -eq 0 ]] || { echo "run this script with sudo" >&2; exit 1; }
[[ $(hostname) == efis ]] || { echo "wrong node: expected efis" >&2; exit 1; }
[[ -f "$candidate" ]] || { echo "candidate drop-in missing" >&2; exit 1; }

mkdir -p "$evidence"
exec > >(tee -a "$evidence/runtime-path-correction.log") 2>&1
cp -a "$dropin" "$backup"

recover() {
  local rc=$?
  if [[ $success -eq 0 ]]; then
    echo "runtime path correction failed (rc=$rc); restoring previous drop-in" >&2
    install -m 0644 "$backup" "$dropin"
    systemctl daemon-reload || true
    systemctl restart "$service" || true
  fi
  exit "$rc"
}
trap recover ERR

install -d -o pi -g pi -m 0755 "$runtime" "$runtime/captures" "$runtime/legacy-import"
for legacy in \
  /home/pi/efis-next/state/engine-runtime.state \
  /home/pi/efis-next/state/engine-runtime.state.bak; do
  if [[ -e "$legacy" ]]; then
    install -o pi -g pi -m 0644 "$legacy" "$runtime/legacy-import/$(basename "$legacy")"
  fi
done
if [[ ! -e "$runtime/blackbox-fuel-events.jsonl" && \
      -f /home/pi/efis-next/state/blackbox-fuel-events.jsonl ]]; then
  install -o pi -g pi -m 0644 /home/pi/efis-next/state/blackbox-fuel-events.jsonl \
    "$runtime/blackbox-fuel-events.jsonl"
fi

install -m 0644 "$candidate" "$dropin"
systemctl daemon-reload
systemd-analyze verify "$service"
started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
systemctl restart "$service"
sleep 15
systemctl is-active --quiet "$service"
pid=$(systemctl show "$service" -p MainPID --value)
[[ $(readlink -f "/proc/$pid/exe") == /home/pi/efis-next-releases/ems-7655799-v1/bin/efis-ems-qt ]]
test -s "$runtime/engine-runtime.state"
if journalctl -u "$service" --since "$started" --no-pager \
    | grep -Eq 'engine_runtime_(save_failed|rejected)'; then
  echo "runtime persistence error remains" >&2
  exit 1
fi

systemctl show "$service" -p ActiveState -p SubState -p MainPID -p NRestarts \
  > "$evidence/status-runtime-corrected.txt"
sha256sum "$runtime/engine-runtime.state" > "$evidence/engine-runtime-state.sha256"
stat -c '%a %U:%G %s %n' "$runtime/engine-runtime.state" \
  > "$evidence/runtime-files.txt"
success=1
trap - ERR
echo "EMS_RUNTIME_PATH_CORRECTION=PASS" | tee "$evidence/runtime-path-result.txt"
