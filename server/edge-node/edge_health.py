#!/usr/bin/env python3
"""Read-only health API for the EFIS EDGE V2 shadow node."""
import argparse
import json
import os
import platform
import shutil
import subprocess
import tempfile
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

VERSION = "0.1.0-shadow"


def read_text(path, default=""):
    try:
        return Path(path).read_text(errors="replace").replace("\0", "").strip()
    except OSError:
        return default


def command(*args):
    try:
        return subprocess.run(args, check=False, capture_output=True, text=True, timeout=3).stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return ""


def service_state(name):
    state = command("systemctl", "is-active", name)
    return state if state else "unknown"


def network_link(name):
    raw = command("ip", "-json", "-details", "-statistics", "link", "show", name)
    try:
        item = json.loads(raw)[0]
        stats = item.get("stats64") or item.get("stats") or {}
        return {"present": True, "operstate": item.get("operstate", "UNKNOWN"),
                "flags": item.get("flags", []), "rx": stats.get("rx", {}),
                "tx": stats.get("tx", {}), "details": item.get("linkinfo", {}).get("info_data", {})}
    except (ValueError, IndexError, KeyError):
        return {"present": False, "operstate": "MISSING", "flags": [], "rx": {}, "tx": {}, "details": {}}


def snapshot(root="/"):
    usage = shutil.disk_usage(root)
    temperature = read_text("/sys/class/thermal/thermal_zone0/temp", "0")
    try: temperature_c = round(int(temperature) / 1000.0, 1)
    except ValueError: temperature_c = None
    can_a, can_b = network_link("can_a"), network_link("can_b")
    critical_ok = service_state("bbxlog.service") == "active" and can_a["present"]
    return {
        "schema": "efis-next.edge-health.v1", "version": VERSION,
        "mode": "SHADOW_READ_ONLY", "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "node": {"hostname": platform.node(), "architecture": platform.machine(),
                 "model": read_text("/proc/device-tree/model", "unknown")},
        "storage": {"root": root, "total_bytes": usage.total, "used_bytes": usage.used,
                    "free_bytes": usage.free, "used_percent": round(usage.used/usage.total*100, 2)},
        "thermal": {"temperature_c": temperature_c},
        "services": {"bbxlog": service_state("bbxlog.service"),
                     "bbxmqtt": service_state("bbxmqtt.service"),
                     "chrony": service_state("chrony.service")},
        "can": {"can_a": can_a, "can_b": can_b},
        "policy": {"can_transmit_allowed": False, "flight_critical_dependency": False},
        "health": "OK" if critical_ok and usage.free > 20*1024**3 else "DEGRADED",
    }


def atomic_json(path, payload):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=path.name+".", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, indent=2); stream.write("\n"); stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


class Handler(BaseHTTPRequestHandler):
    output = None
    def do_GET(self):
        if self.path not in ("/v1/health", "/v1/status"):
            self.send_error(404); return
        payload = snapshot()
        if self.output: atomic_json(self.output, payload)
        body = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(200); self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
        self.end_headers(); self.wfile.write(body)
    def log_message(self, fmt, *args):
        return


def main():
    parser=argparse.ArgumentParser(); parser.add_argument("--once",action="store_true")
    parser.add_argument("--output",default="run/edge-health.json"); parser.add_argument("--bind",default="127.0.0.1")
    parser.add_argument("--port",type=int,default=18091); args=parser.parse_args()
    payload=snapshot(); atomic_json(args.output,payload)
    if args.once: print(json.dumps(payload)); return
    Handler.output=args.output
    ThreadingHTTPServer((args.bind,args.port),Handler).serve_forever()

if __name__=="__main__": main()
