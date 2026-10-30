#!/bin/bash
# Explicit 15-minute isolated PFD bench. Never installs/replaces an avionics release.
set -euo pipefail
BASE=/home/efis/efis-sil-panel-r117
GUARD=/run/efis-sil-r117/guard.sh
ORIGINAL=/home/efis/efis-next-releases/pfdrec-7051a47-retention-v1/bin/efis-pfd-qt
TOOL=$BASE/build-v3/efis-sil-panel-replay
FRAMES_SHA=697ffb68a76d0b20c448e4d3b05194f1d3016f5615a95247f4589c900d4d8b80

restore() {
    sudo -n /bin/bash "$BASE/run_pfd_sil_bench_r117.sh" --root-restore
}
preflight() {
    test "$(id -un)" = efis
    test "$(hostname)" = efis-pfd-01
    sudo -n true
    test "$(vcgencmd get_throttled)" = throttled=0x0
    test "$(readlink -f /home/efis/efis-next-current/bin/efis-pfd-qt)" = "$ORIGINAL"
    test "$(sha256sum "$ORIGINAL" | cut -d' ' -f1)" = a64edcf2d0fcf5f64f30d771926a60d4e0c2854763a2f082375c7630df828854
    test "$(sha256sum "$TOOL" | cut -d' ' -f1)" = 81bc503f6063e07ec57415a6fe8a4f383f5966411aaf5a994198f79a8f440d37
    test -S /run/user/1000/wayland-0
    systemctl --user is-active --quiet efis-pfd.service
    test -f "$BASE/panel-profile.json"
    "$TOOL" "$BASE/replay/nominal-v1" "$FRAMES_SHA" --check
    echo PFD_BENCH_PREFLIGHT=PASS
}
case "${1:-}" in
--root-restore)
    test "$(id -u)" = 0
    systemctl stop efis-sil-panel-feed.service efis-sil-panel-screen.service 2>/dev/null || true
    /usr/sbin/runuser -u efis -- /usr/bin/env XDG_RUNTIME_DIR=/run/user/1000 \
        DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus /usr/bin/systemctl --user start efis-pfd.service
    /usr/sbin/runuser -u efis -- /usr/bin/env XDG_RUNTIME_DIR=/run/user/1000 \
        DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus /usr/bin/systemctl --user is-active --quiet efis-pfd.service
    systemctl stop efis-sil-panel-restore.timer 2>/dev/null || true
    echo PFD_ORIGINAL_RESTORED=PASS
    exit 0 ;;
--restore) restore; exit 0 ;;
--preflight) preflight; exit 0 ;;
--activate) ;;
*) echo 'usage: run_pfd_sil_bench_r117.sh --preflight|--activate|--restore' >&2; exit 2 ;;
esac
preflight
test ! -e "$BASE/work-v1" || test -d "$BASE/work-v1/config"
if [ ! -d "$BASE/work-v1" ]; then
    mkdir -m 0700 "$BASE/work-v1"
    cp -r --no-preserve=ownership,mode /home/efis/efis-next-current/config "$BASE/work-v1/config"
    # Immutable root-owned data can be read, but cannot be changed by the bench user.
    ln -s /home/efis/efis-next-current/data "$BASE/work-v1/data"
fi
mkdir -p "$BASE/work-v1/state" "$BASE/work-v1/settings" "$BASE/work-v1/cache"
sudo -n install -d -o root -g root -m 0700 /run/efis-sil-r117
sudo -n install -o root -g root -m 0500 "$BASE/run_pfd_sil_bench_r117.sh" "$GUARD"
sudo -n systemctl stop efis-sil-panel-restore.timer 2>/dev/null || true
sudo -n systemd-run --collect --unit=efis-sil-panel-restore --on-active=930 \
    --timer-property=AccuracySec=1 /bin/bash "$GUARD" --root-restore
sudo -n systemctl is-active --quiet efis-sil-panel-restore.timer
trap 'rc=$?; trap - ERR; restore; exit "$rc"' ERR
systemctl --user stop efis-pfd.service
sudo -n systemd-run --collect --unit=efis-sil-panel-screen --uid=efis \
    --property=PrivateNetwork=yes \
    --setenv=XDG_RUNTIME_DIR=/run/user/1000 --setenv=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus \
    --property=WorkingDirectory="$BASE/work-v1" --property=RuntimeMaxSec=900 \
    --property=TimeoutStopSec=5 \
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
sudo -n systemd-run --collect --unit=efis-sil-panel-feed --uid=efis \
    --property=PrivateNetwork=yes --property=JoinsNamespaceOf=efis-sil-panel-screen.service \
    --property=RuntimeMaxSec=900 --property=TimeoutStopSec=3 \
    --property='ExecStopPost=+/usr/bin/systemctl stop efis-sil-panel-screen.service' \
    "$TOOL" "$BASE/replay/nominal-v1" "$FRAMES_SHA" --loopback-900s
sudo -n systemctl is-active --quiet efis-sil-panel-screen.service
sudo -n systemctl is-active --quiet efis-sil-panel-feed.service
echo PFD_BENCH_ACTIVE=TEMPORARY_15_MINUTES
echo SOURCE=REPLAY_JSBSIM_SIM_NODE_65000_LOOPBACK_49152
echo EMS_WEATHER_AIRFRAME=NOT_CONNECTED
echo FLIGHT_APPROVAL=false
