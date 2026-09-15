#!/bin/sh
# Persistently mute QGroundControl synthetic speech without stopping telemetry.

set -eu

systemctl --user mask --now speech-dispatcher.service speech-dispatcher.socket

for unit in speech-dispatcher.service speech-dispatcher.socket; do
    test "$(systemctl --user is-enabled "$unit" 2>/dev/null || true)" = "masked"
    test "$(systemctl --user is-active "$unit" 2>/dev/null || true)" = "inactive"
done

systemctl --user is-active --quiet efis-qgc-monitor.service
ss -H -lunp | grep -E ':14550[[:space:]].*QGroundControl' >/dev/null

echo "QGC_SYNTHETIC_SPEECH=MUTED"
echo "QGC_TELEMETRY=ACTIVE"
