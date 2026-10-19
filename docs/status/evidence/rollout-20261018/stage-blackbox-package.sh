#!/usr/bin/env bash
set -Eeuo pipefail
[[ $(id -un) == pi && $(hostname) == efisblackbox && $(uname -m) == aarch64 ]]
stage=/home/pi/efis-release-blackbox-a0be9a5-stage
release=$stage/blackbox-a0be9a5-main-v1
rollback=/opt/efis-blackbox-releases/blackbox-7655799-v1
[[ $(readlink -f /opt/efis-blackbox-current) == "$rollback" ]]
[[ ! -e "$release" && ! -L "$release" ]]
pid=$(systemctl show efis-blackbox-broker.service -p MainPID --value)
[[ "$pid" =~ ^[1-9][0-9]*$ ]]
[[ $(df --output=avail -B1 /home/pi | tail -n 1 | tr -d ' ') -ge 8589934592 ]]
echo "85bf9aad2124d5e75504fe772f5fae69e319b0d6ce5515fc7e6a611638b87512  $stage/blackbox-a0be9a5-main-v1.tar.gz" | sha256sum -c --strict
tar -xzf "$stage/blackbox-a0be9a5-main-v1.tar.gz" -C "$stage"
export PYTHONPATH="$release"
for path in "$release" "$rollback"; do
  python3 "$release/tools/deployment/validate_release_manifest.py" \
    --manifest "$path/release-manifest.json" --release-root "$path" \
    --expected-role BLACKBOX --expected-platform RPI5_AARCH64 --check-files
done
ldd "$release/bin/efis-blackbox-broker"
if ldd "$release/bin/efis-blackbox-broker" | grep -q 'not found'; then exit 1; fi
[[ $(readlink -f /opt/efis-blackbox-current) == "$rollback" ]]
[[ $(systemctl show efis-blackbox-broker.service -p MainPID --value) == "$pid" ]]
[[ $(systemctl is-active efis-blackbox-broker.service) == active ]]
systemctl show efis-blackbox-broker.service -p MainPID -p NRestarts -p ActiveState
sha256sum "$release/release-manifest.json" "$release/bin/efis-blackbox-broker"
vcgencmd get_throttled
echo BLACKBOX_STAGING=PASS
echo LIVE_BUS_ACCESS=NOT_RUN
echo ACTIVATION=NOT_RUN_POWER_GATE
