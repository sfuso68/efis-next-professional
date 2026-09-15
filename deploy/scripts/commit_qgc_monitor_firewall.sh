#!/bin/sh
# Commit a QGC firewall trial only after local health gates remain true.
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "error: run with sudo" >&2
    exit 77
fi

systemctl is-enabled --quiet efis-qgc-monitor-firewall.service
systemctl is-active --quiet efis-qgc-monitor-firewall.service
/usr/sbin/nft list table inet efis_qgc_monitor >/dev/null
ss -H -lunp | grep -F ':14550' | grep -Fq 'QGroundControl'

# Cancel rollback only after every verification above has passed.
systemctl stop efis-qgc-firewall-rollback.timer 2>/dev/null || true
systemctl stop efis-qgc-firewall-rollback.service 2>/dev/null || true
systemctl reset-failed efis-qgc-firewall-rollback.service 2>/dev/null || true
echo "QGC monitor firewall committed"
