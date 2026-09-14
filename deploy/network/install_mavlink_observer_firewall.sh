#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "error: run this installer as root" >&2
    exit 1
fi

efis_stage_dir=/home/sfuso
efis_config_dir=/etc/efis
efis_unit_dir=/etc/systemd/system
efis_rule_source=${efis_stage_dir}/efis-mavlink-observer-server.nft
efis_unit_source=${efis_stage_dir}/efis-mavlink-observer-firewall.service
efis_rule_target=${efis_config_dir}/mavlink-observer-firewall.nft
efis_unit_target=${efis_unit_dir}/efis-mavlink-observer-firewall.service

test -r "${efis_rule_source}"
test -r "${efis_unit_source}"
test -x /usr/sbin/nft
test -x /usr/bin/systemctl
test -x /usr/bin/systemd-analyze

/usr/bin/install -d -m 0755 "${efis_config_dir}"
/usr/bin/install -m 0644 "${efis_rule_source}" "${efis_rule_target}"
/usr/bin/install -m 0644 "${efis_unit_source}" "${efis_unit_target}"

/usr/sbin/nft --check --file "${efis_rule_target}"
/usr/bin/systemd-analyze verify "${efis_unit_target}"
/usr/bin/systemctl daemon-reload
/usr/bin/systemctl enable --now efis-mavlink-observer-firewall.service

/usr/bin/systemctl is-enabled efis-mavlink-observer-firewall.service
/usr/bin/systemctl is-active efis-mavlink-observer-firewall.service
/usr/sbin/nft list table inet efis_mavlink_observer
