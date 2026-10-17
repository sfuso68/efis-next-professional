#!/bin/sh
set -eu
base=/home/sfuso/.local/lib/avionix-dashboard
units=/home/sfuso/.config/systemd/user
mkdir -p "$units"
cat > "$units/avionix-dashboard.service" <<'EOF'
[Unit]
Description=AVIONIX private read-only project dashboard (SSH tunnel)
[Service]
WorkingDirectory=/home/sfuso/.local/lib/avionix-dashboard
ExecStart=/usr/bin/python3 -m tools.project_status.serve_dashboard
Restart=on-failure
RestartSec=5
NoNewPrivileges=yes
PrivateTmp=yes
[Install]
WantedBy=default.target
EOF
cat > "$units/avionix-dashboard-refresh.service" <<'EOF'
[Unit]
Description=Refresh AVIONIX dashboard from GitHub main (read-only)
[Service]
Type=oneshot
WorkingDirectory=/home/sfuso/.local/lib/avionix-dashboard
ExecStart=/usr/bin/python3 -m tools.project_status.serve_dashboard --refresh
NoNewPrivileges=yes
PrivateTmp=yes
EOF
cat > "$units/avionix-dashboard-refresh.timer" <<'EOF'
[Unit]
Description=Refresh AVIONIX project status every five minutes
[Timer]
OnBootSec=1min
OnUnitActiveSec=5min
Persistent=true
[Install]
WantedBy=timers.target
EOF
cd "$base"
python3 -m tools.project_status.serve_dashboard --refresh
systemctl --user daemon-reload
systemctl --user enable --now avionix-dashboard.service avionix-dashboard-refresh.timer
systemctl --user start avionix-dashboard-refresh.service
