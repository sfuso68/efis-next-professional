#!/bin/sh
# Controlled Ground Station Wi-Fi fault with recovery armed before disconnect.
set -eu

export LC_ALL=C

mode=run
if [ "${1:-}" = "--check" ]; then
    mode=check
    shift
fi

interface=${1:-wlo1}
connection=${2:-CASA}
fault_seconds=${3:-45}
recovery_unit=efis-wifi-recovery

for command in nmcli systemctl systemd-run; do
    command -v "$command" >/dev/null 2>&1 || {
        echo "error: required command not found: $command" >&2
        exit 69
    }
done

case "$fault_seconds" in
    *[!0-9]*|'')
        echo "error: duration must be an integer between 20 and 180 seconds" >&2
        exit 64
        ;;
esac

if [ "$fault_seconds" -lt 20 ] || [ "$fault_seconds" -gt 180 ]; then
    echo "error: duration must be between 20 and 180 seconds" >&2
    exit 64
fi

if [ "$(nmcli -g GENERAL.TYPE device show "$interface" 2>/dev/null || true)" != "wifi" ]; then
    echo "error: interface $interface is not a NetworkManager Wi-Fi device" >&2
    exit 69
fi

connection_uuid=$(nmcli -g connection.uuid connection show "$connection" 2>/dev/null || true)
case "$connection_uuid" in
    ''|*"
"*)
        echo "error: connection $connection must resolve to exactly one UUID" >&2
        exit 69
        ;;
esac

if ! nmcli -t -f DEVICE,STATE device status | grep -Fqx "$interface:connected"; then
    echo "error: interface $interface is not connected" >&2
    exit 69
fi

if ! nmcli -t -f UUID,DEVICE connection show --active \
    | grep -Fqx "$connection_uuid:$interface"; then
    echo "error: connection $connection is not active on $interface" >&2
    exit 69
fi

if [ "$mode" = "check" ]; then
    echo "WIFI_FAULT_PREFLIGHT=PASS"
    echo "interface=$interface"
    echo "connection=$connection"
    echo "uuid=$connection_uuid"
    echo "fault_seconds=$fault_seconds"
    exit 0
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "error: run as root (sudo); NetworkManager requires authorization" >&2
    exit 77
fi

# Remove only a stale unit with the dedicated test name.  No network state is
# changed until systemd has accepted the recovery timer.
systemctl stop "$recovery_unit.timer" "$recovery_unit.service" 2>/dev/null || true
systemctl reset-failed "$recovery_unit.service" 2>/dev/null || true

systemd-run \
    --unit="$recovery_unit" \
    --on-active="${fault_seconds}s" \
    --timer-property=AccuracySec=1s \
    --collect \
    /usr/bin/nmcli connection up uuid "$connection_uuid" ifname "$interface"

systemctl is-active --quiet "$recovery_unit.timer"
echo "recovery armed: connection=$connection uuid=$connection_uuid interface=$interface in=${fault_seconds}s"
echo "disconnecting $interface now"

nmcli device disconnect "$interface"
