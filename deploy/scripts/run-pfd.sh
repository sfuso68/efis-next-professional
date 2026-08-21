#!/bin/sh
set -eu

project_dir=/home/efis/efis-next
log_dir="$project_dir/logs"
mkdir -p "$log_dir"

export QT_QPA_PLATFORM=wayland
export WAYLAND_DISPLAY=wayland-0
export EFIS_KIOSK=1
export QSG_RENDER_LOOP=threaded

printf '%s event=launcher_start\n' "$(date --iso-8601=seconds)" >> "$log_dir/pfd.log"
exec "$project_dir/build/dev/efis-pfd-qt" >> "$log_dir/pfd.log" 2>&1
