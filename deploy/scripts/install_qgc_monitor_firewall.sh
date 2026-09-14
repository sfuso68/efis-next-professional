#!/bin/sh
# Install the narrow QGC observer firewall after syntax/policy checks.
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "error: run with sudo" >&2
    exit 77
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
rules_source="$script_dir/../network/efis-qgc-monitor-firewall.nft"
unit_source="$script_dir/../systemd/efis-qgc-monitor-firewall.service"

test -r "$rules_source"
test -r "$unit_source"

install -d -m 0755 /etc/efis
install -m 0644 "$rules_source" /etc/efis/qgc-monitor-firewall.nft
install -m 0644 "$unit_source" /etc/systemd/system/efis-qgc-monitor-firewall.service

/usr/sbin/nft --check --file /etc/efis/qgc-monitor-firewall.nft
systemd-analyze verify /etc/systemd/system/efis-qgc-monitor-firewall.service
systemctl daemon-reload
systemctl enable --now efis-qgc-monitor-firewall.service

systemctl is-enabled efis-qgc-monitor-firewall.service
systemctl is-active efis-qgc-monitor-firewall.service
/usr/sbin/nft list table inet efis_qgc_monitor
