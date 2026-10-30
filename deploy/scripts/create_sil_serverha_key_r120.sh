#!/bin/sh
set -eu
test "$(id -un)" = sfuso
test "$(hostname)" = serverha
KEY=/home/sfuso/.ssh/efis_sil_pfd_ed25519
test ! -e "$KEY" && test ! -e "$KEY.pub"
umask 077
ssh-keygen -q -t ed25519 -N '' -f "$KEY" -C efis-sil-serverha-to-pfd-r120
chmod 600 "$KEY"
ssh-keygen -lf "$KEY.pub"
# Only public material is returned. The private file never leaves serverha.
