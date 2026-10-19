#!/bin/sh
# Exercise the same in-memory hash-checked launcher as the operator command,
# but without sudo and ONLY in read-only mode.
exec /usr/bin/python3 -I -c '
import hashlib, pathlib
p = pathlib.Path("/home/pi/activate_ems_a0be9a5.py")
b = p.read_bytes()
if hashlib.sha256(b).hexdigest() != "7848ef0ee53add17aa8b04445ea6d588f823c3a4ae708e577a901bc6683564ce":
    raise SystemExit("HASH_MISMATCH: non eseguire")
exec(compile(b, str(p), "exec"), {"__name__": "__main__", "__file__": str(p)})
' --check-only
