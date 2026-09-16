#!/bin/sh
set -eu

# Avvio del nodo EMS.
#
# A differenza di run-pfd.sh questo script non fissa il percorso del progetto:
# lo ricava da dove si trova. Il nodo EMS nasce su una macchina che ospita gia'
# l'EFIS vecchio, e su quella macchina l'utente e la cartella non sono quelli
# del banco PFD. Un percorso scritto qui dentro obbligherebbe a modificare un
# file versionato per installare, e la modifica locale sarebbe la prima cosa a
# divergere dal repository.
script_dir=$(cd "$(dirname "$0")" && pwd)
project_dir=$(cd "$script_dir/../.." && pwd)
build_dir="${EFIS_BUILD_DIR:-$project_dir/build/dev}"
binary="$build_dir/efis-ems-qt"

log_dir="$project_dir/logs"
mkdir -p "$log_dir"

if [ ! -x "$binary" ]; then
    printf '%s event=launcher_abort reason=missing_binary path=%s\n' \
        "$(date --iso-8601=seconds)" "$binary" >> "$log_dir/ems.log"
    printf 'efis-ems-qt non trovato in %s\n' "$binary" >&2
    exit 1
fi

# La piattaforma la decide il nodo, non questo script: il RPi4 disegna su eglfs
# perche' non ha compositore, un nodo con Wayland la imposta a wayland. Il
# valore arriva dall'unita' systemd, che e' il posto dove la differenza fra due
# macchine e' leggibile.
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-eglfs}"
if [ "$QT_QPA_PLATFORM" = wayland ]; then
    export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
fi
export EFIS_KIOSK=1
export QSG_RENDER_LOOP="${QSG_RENDER_LOOP:-threaded}"
# Il nodo EMS di banco ascolta soltanto sull'interfaccia FLIGHT-DATA. I valori
# possono essere sostituiti dal servizio, ma non devono ricadere su 0.0.0.0 o
# sulla WLAN di recovery se il drop-in locale non e' ancora installato.
export EFIS_EDP_BIND="${EFIS_EDP_BIND:-10.77.10.12}"
export EFIS_EDP_PORT="${EFIS_EDP_PORT:-49003}"
export EFIS_EDP_SOURCE_NODE="${EFIS_EDP_SOURCE_NODE:-47}"

printf '%s event=launcher_start node=ems binary=%s\n' \
    "$(date --iso-8601=seconds)" "$binary" >> "$log_dir/ems.log"
exec "$binary" >> "$log_dir/ems.log" 2>&1
