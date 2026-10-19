#!/usr/bin/env bash
# Complete validation after the legacy rollback lacked its own validator.
set -Eeuo pipefail
[[ $(id -un) == pi && $(hostname) == pi5 && $(uname -m) == aarch64 ]]
release=/home/pi/efis-next-release-mfd-a0be9a5-v1
rollback=/home/pi/efis-next-release-mfd-3432402-v1
validator=$release/tools/deployment/validate_release_manifest.py
[[ $(readlink -f /home/pi/efis-next-current) == "$rollback" ]]
date -u
for path in "$release" "$rollback"; do
  python3 "$validator" --manifest "$path/release-manifest.json" \
    --release-root "$path" --expected-role MFD --expected-platform RPI5_AARCH64 --check-files
done
ldd "$release/bin/efis-pfd-qt"
if ldd "$release/bin/efis-pfd-qt" | grep -q 'not found'; then exit 1; fi
sha256sum "$release/bin/efis-pfd-qt" "$release/release-manifest.json"
systemctl show efis-next-mfd-direct.service -p MainPID -p NRestarts -p ActiveState
[[ $(systemctl show efis-next-mfd-direct.service -p MainPID --value) == 1395 ]]
[[ $(systemctl show efis-next-mfd-direct.service -p NRestarts --value) == 0 ]]
[[ $(systemctl is-active efis-next-mfd-direct.service) == active ]]
vcgencmd get_throttled
vcgencmd measure_temp
echo STAGING=PASS
echo ACTIVATION=NOT_RUN_POWER_GATE
echo VISUAL_ACCEPTANCE=NOT_RUN
echo FLIGHT_APPROVAL=false
