#!/bin/sh
# Persistently mute QGroundControl synthetic speech without stopping telemetry.

set -eu

settings="${HOME}/.config/QGroundControl/QGroundControl.ini"
test -r "$settings"

systemctl --user stop efis-qgc-monitor.service

if grep -q '^audioMuted=' "$settings"; then
    sed -i 's/^audioMuted=.*/audioMuted=true/' "$settings"
else
    grep -q '^\[General\]$' "$settings"
    sed -i '/^\[General\]$/a audioMuted=true' "$settings"
fi

systemctl --user mask --now speech-dispatcher.service speech-dispatcher.socket
pkill -TERM -u "$(id -u)" -f '(^|/)speech-dispatcher([[:space:]]|$)' 2>/dev/null || true

systemctl --user daemon-reload
systemctl --user start efis-qgc-monitor.service

attempt=0
while [ "$attempt" -lt 30 ]; do
    if systemctl --user is-active --quiet efis-qgc-monitor.service && \
       ss -H -lunp | grep -E ':14550[[:space:]].*QGroundControl' >/dev/null; then
        break
    fi
    attempt=$((attempt + 1))
    sleep 1
done

for unit in speech-dispatcher.service speech-dispatcher.socket; do
    test "$(systemctl --user is-enabled "$unit" 2>/dev/null || true)" = "masked"
    test "$(systemctl --user is-active "$unit" 2>/dev/null || true)" = "inactive"
done

systemctl --user is-active --quiet efis-qgc-monitor.service
ss -H -lunp | grep -E ':14550[[:space:]].*QGroundControl' >/dev/null
grep -qx 'audioMuted=true' "$settings"
if pgrep -u "$(id -u)" -f '(^|/)speech-dispatcher([[:space:]]|$)' >/dev/null; then
    echo "error: speech-dispatcher was spawned despite mute policy" >&2
    exit 1
fi

echo "QGC_SYNTHETIC_SPEECH=MUTED"
echo "QGC_TELEMETRY=ACTIVE"
