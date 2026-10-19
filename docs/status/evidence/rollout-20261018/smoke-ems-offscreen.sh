#!/usr/bin/env bash
set -Eeuo pipefail
[[ $(id -un) == pi && $(hostname) == efis && $(uname -m) == aarch64 ]]
release=/home/pi/efis-next-releases/ems-a0be9a5-main-v1
smoke=/home/pi/efis-release-ems-a0be9a5-stage/smoke
[[ ! -e "$smoke" && ! -L "$smoke" ]]
mkdir -p "$smoke/config" "$smoke/cache" "$smoke/data" "$smoke/state"
cd "$smoke"
# No real EDP endpoint or display. All settings and test writes are isolated.
unset EFIS_EDP_BIND EFIS_EDP_PORT EFIS_KIOSK
export XDG_CONFIG_HOME="$smoke/config" XDG_CACHE_HOME="$smoke/cache"
export XDG_DATA_HOME="$smoke/data" XDG_STATE_HOME="$smoke/state"
export EFIS_AIRCRAFT_PROFILE="$release/config/active/aircraft-profile.json"
export EFIS_CATALOG_INDEX="$release/data/catalog/efis-catalog-index.json"
export EFIS_OPERATIONAL_PROFILE_DRAFT="$smoke/state/draft.json"
export EFIS_CENTRAL_BLACKBOX_CACHE="$smoke/state/central-cache.json"
export EFIS_CENTRAL_BLACKBOX_REQUEST="$smoke/state/maintenance.jsonl"
export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
export EFIS_CAPTURE="$smoke/ems-layout.png" EFIS_CAPTURE_SIZE=1024x600 EFIS_CAPTURE_DELAY_MS=1200
timeout 20s "$release/bin/efis-ems-qt"
[[ -s "$smoke/ems-layout.png" ]]
sha256sum "$smoke/ems-layout.png"
[[ $(systemctl show efis-ems.service -p MainPID --value) == 949 ]]
[[ $(systemctl is-active efis-ems.service) == active ]]
echo EMS_QML_STARTUP=PASS
echo PHYSICAL_GAUGE_RENDERING=NOT_VERIFIED_SOFTWARE_BACKEND
echo ACTIVATION=NOT_RUN
