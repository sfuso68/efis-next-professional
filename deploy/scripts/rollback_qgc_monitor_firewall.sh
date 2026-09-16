#!/bin/sh
# Remove only the dedicated QGC monitor firewall after an uncommitted trial.
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "error: run with sudo" >&2
    exit 77
fi

systemctl disable --now efis-qgc-monitor-firewall.service 2>/dev/null || true
/usr/sbin/nft delete table inet efis_qgc_monitor 2>/dev/null || true
echo "QGC monitor firewall trial rolled back"
