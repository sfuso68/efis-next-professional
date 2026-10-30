#!/bin/bash
# Specific PFD bench: original renderer, challenge-fed loopback, <=140s window.
set -euo pipefail
BASE=/home/efis/efis-sil-panel-r117
SELF=$BASE/run_pfd_sil_live_r119.sh
GUARD=/run/efis-sil-r119/guard.sh
ORIGINAL=/home/efis/efis-next-releases/pfdrec-7051a47-retention-v1/bin/efis-pfd-qt
TOOL=$BASE/r119-build-v4/efis-sil-panel-replay
case "${1:-}" in
--root-restore)
    test "$(id -u)" = 0
    systemctl stop efis-sil-r120-feed.service efis-sil-live-feed.service efis-sil-panel-screen.service 2>/dev/null || true
    /usr/sbin/runuser -u efis -- /usr/bin/env XDG_RUNTIME_DIR=/run/user/1000 \
      DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus /usr/bin/systemctl --user start efis-pfd.service
    /usr/sbin/runuser -u efis -- /usr/bin/env XDG_RUNTIME_DIR=/run/user/1000 \
      DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus /usr/bin/systemctl --user is-active --quiet efis-pfd.service
    systemctl stop efis-sil-r119-restore.timer 2>/dev/null || true
    echo R119_ORIGINAL_RESTORED=PASS
    exit 0 ;;
--restore) sudo -n /bin/bash "$SELF" --root-restore; exit 0 ;;
--preflight|--activate) ;;
*) echo 'usage: run_pfd_sil_live_r119.sh --preflight|--activate|--restore' >&2; exit 2 ;;
esac
test "$(id -un)" = efis && test "$(hostname)" = efis-pfd-01
sudo -n true
test "$(vcgencmd get_throttled)" = throttled=0x0
test "$(readlink -f /home/efis/efis-next-current/bin/efis-pfd-qt)" = "$ORIGINAL"
test "$(sha256sum "$ORIGINAL" | cut -d' ' -f1)" = a64edcf2d0fcf5f64f30d771926a60d4e0c2854763a2f082375c7630df828854
test "$(sha256sum "$TOOL" | cut -d' ' -f1)" = 7d1c5880e6e06d69f71b6c3fe26a8f67860a65fe2259f661c49ad960066f6167
test -S /run/user/1000/wayland-0
test -f "$BASE/panel-profile.json" && test -d "$BASE/work-v1/config"
systemctl --user is-active --quiet efis-pfd.service
for unit in efis-sil-panel-screen.service efis-sil-panel-feed.service efis-sil-live-feed.service efis-sil-r120-check.service efis-sil-r120-feed.service; do
    if sudo -n systemctl is-active --quiet "$unit"; then exit 2; fi
done
"$TOOL" "$BASE/replay/nominal-v1" 697ffb68a76d0b20c448e4d3b05194f1d3016f5615a95247f4589c900d4d8b80 --check
echo R119_PREFLIGHT=PASS
test "$1" = --activate || exit 0
sudo -n install -d -o root -g root -m 0700 /run/efis-sil-r119
sudo -n install -o root -g root -m 0500 "$SELF" "$GUARD"
sudo -n systemd-run --collect --unit=efis-sil-r119-restore --on-active=180 \
    --timer-property=AccuracySec=1 /bin/bash "$GUARD" --root-restore
sudo -n systemctl is-active --quiet efis-sil-r119-restore.timer
trap 'rc=$?; trap - ERR; sudo -n /bin/bash "$SELF" --root-restore; exit "$rc"' ERR
systemctl --user stop efis-pfd.service
sudo -n systemd-run --collect --unit=efis-sil-panel-screen --uid=efis \
    --property=PrivateNetwork=yes --property=RuntimeMaxSec=140 --property=TimeoutStopSec=5 \
    --property=WorkingDirectory="$BASE/work-v1" \
    --setenv=XDG_RUNTIME_DIR=/run/user/1000 --setenv=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus \
    --property='ExecStopPost=/usr/bin/systemctl --user start efis-pfd.service' \
    /usr/bin/env -i HOME=/home/efis USER=efis PATH=/usr/local/bin:/usr/bin:/bin \
    XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 DISPLAY=:0 \
    DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus \
    QT_QPA_PLATFORM=wayland QT_QPA_PLATFORMTHEME=qt6ct EFIS_KIOSK=1 \
    XDG_CONFIG_HOME="$BASE/work-v1/settings" XDG_CACHE_HOME="$BASE/work-v1/cache" \
    EFIS_AIRCRAFT_PROFILE="$BASE/panel-profile.json" \
    EFIS_DDP_BIND_ADDRESS=127.0.0.1 EFIS_DDP_LISTEN_PORT=49152 EFIS_DDP_SOURCE_NODE=65000 \
    EFIS_SIMULATION_ROLE=DISABLED EFIS_START_PAGE=PFD \
    EFIS_TERRAIN_GPU_ROOT=/home/efis/efis-next/data/terrain/gpu \
    EFIS_TERRAIN_ACTIVE=/home/efis/efis-next/data/terrain/active-package.json \
    EFIS_IMAGERY_TEXTURE_ROOT=/home/efis/efis-next/output/imagery/active-textures \
    EFIS_IMAGERY_ACTIVE_PACKAGE=/home/efis/efis-next/output/imagery/active-package.json \
    EFIS_WEATHER_SIMULATION_DIR="$BASE/work-v1/cache" "$ORIGINAL"
sudo -n systemctl is-active --quiet efis-sil-panel-screen.service
echo R119_SCREEN_READY=140S_LEASE_WAITING_FOR_LIVE_FEED
