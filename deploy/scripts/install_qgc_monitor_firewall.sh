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
rollback_source="$script_dir/rollback_qgc_monitor_firewall.sh"
commit_source="$script_dir/commit_qgc_monitor_firewall.sh"

test -r "$rules_source"
test -r "$unit_source"
test -r "$rollback_source"
test -r "$commit_source"

install -d -m 0755 /etc/efis
install -m 0644 "$rules_source" /etc/efis/qgc-monitor-firewall.nft
install -m 0644 "$unit_source" /etc/systemd/system/efis-qgc-monitor-firewall.service
install -m 0755 "$rollback_source" /usr/local/sbin/rollback_qgc_monitor_firewall
install -m 0755 "$commit_source" /usr/local/sbin/commit_qgc_monitor_firewall

/usr/sbin/nft --check --file /etc/efis/qgc-monitor-firewall.nft
systemd-analyze verify /etc/systemd/system/efis-qgc-monitor-firewall.service
systemctl daemon-reload

# Arm recovery before changing the active firewall. If acceptance is not
# committed, the dedicated table is removed automatically after five minutes.
systemctl stop efis-qgc-firewall-rollback.timer \
  efis-qgc-firewall-rollback.service 2>/dev/null || true
systemctl reset-failed efis-qgc-firewall-rollback.service 2>/dev/null || true
systemd-run \
  --unit=efis-qgc-firewall-rollback \
  --on-active=300s \
  --timer-property=AccuracySec=1s \
  --collect \
  /usr/local/sbin/rollback_qgc_monitor_firewall
systemctl is-active --quiet efis-qgc-firewall-rollback.timer

systemctl enable --now efis-qgc-monitor-firewall.service

systemctl is-enabled efis-qgc-monitor-firewall.service
systemctl is-active efis-qgc-monitor-firewall.service
/usr/sbin/nft list table inet efis_qgc_monitor
echo "rollback armed for 300 seconds"
echo "after external verification run: sudo /usr/local/sbin/commit_qgc_monitor_firewall"
