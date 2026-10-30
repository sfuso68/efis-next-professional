#!/bin/sh
# Forced command for ONE dedicated serverha key. Never eval original command.
set -eu
case "${SSH_CONNECTION:-}" in
    '192.168.1.6 '*) ;;
    *) echo SIL_ACCESS_DENIED >&2; exit 126 ;;
esac
case "${SSH_ORIGINAL_COMMAND:-}" in
    SIL_CHECK_V1) mode=check ;;
    SIL_FEED_V1) mode=feed ;;
    *) echo SIL_ACCESS_DENIED >&2; exit 126 ;;
esac
exec /usr/bin/env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
    /usr/bin/sudo -n /usr/local/lib/efis-next/sil-r120/run-adapter.sh "$mode"
