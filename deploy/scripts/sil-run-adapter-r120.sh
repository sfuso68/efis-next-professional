#!/bin/sh
# Root helper: two fixed modes, immutable adapter, unprivileged sandbox.
set -eu
test "$(id -u)" = 0
test "$#" = 1
ADAPTER=/usr/local/lib/efis-next/sil-r120/adapter
test "$(sha256sum "$ADAPTER" | cut -d' ' -f1)" = 7d1c5880e6e06d69f71b6c3fe26a8f67860a65fe2259f661c49ad960066f6167
case "$1" in
check)
    unit=efis-sil-r120-check
    mode=--live-check-stdio
    set -- ;;
feed)
    unit=efis-sil-r120-feed
    mode=--live-loopback-stdio
    systemctl is-active --quiet efis-sil-panel-screen.service
    test "$(systemctl show efis-sil-panel-screen.service -p PrivateNetwork --value)" = yes
    test "$(systemctl show efis-sil-panel-screen.service -p User --value)" = efis
    command=$(systemctl show efis-sil-panel-screen.service -p ExecStart --value)
    for required in EFIS_DDP_BIND_ADDRESS=127.0.0.1 EFIS_DDP_LISTEN_PORT=49152 EFIS_DDP_SOURCE_NODE=65000; do
        printf '%s\n' "$command" | grep -Fq -- "$required"
    done
    set -- --property=JoinsNamespaceOf=efis-sil-panel-screen.service \
        --property=ExecStopPost=+/usr/bin/systemctl\ stop\ efis-sil-panel-screen.service ;;
*) echo SIL_ACCESS_DENIED >&2; exit 126 ;;
esac
exec /usr/bin/systemd-run --quiet --pipe --wait --collect --unit="$unit" \
    --property=DynamicUser=yes --property=User=efis-sil-bench \
    --property=PrivateNetwork=yes --property=PrivateDevices=yes --property=PrivateTmp=yes \
    --property=NoNewPrivileges=yes --property=ProtectSystem=strict --property=ProtectHome=yes \
    --property=ProtectKernelTunables=yes --property=ProtectKernelModules=yes \
    --property=ProtectControlGroups=yes --property=ProtectClock=yes --property=ProtectHostname=yes \
    --property=RestrictSUIDSGID=yes --property=RestrictNamespaces=yes --property=RestrictRealtime=yes \
    --property=LockPersonality=yes --property=CapabilityBoundingSet= \
    --property=MemoryMax=128M --property=MemorySwapMax=0 --property=TasksMax=16 --property=CPUQuota=50% \
    --property=RestrictAddressFamilies=AF_UNIX\ AF_INET \
    --property=RuntimeMaxSec=140 --property=TimeoutStopSec=3 --property=UMask=0077 \
    --property=WorkingDirectory=/ --setenv=HOME=/nonexistent "$@" \
    "$ADAPTER" f9296942c9bab6f8738e3b0b3c6c55b49908b0a122805b5103155ede47dcf9bb "$mode"
