#!/usr/bin/env python3
"""Read-only GNSS message audit over MAVLink."""
import argparse
import json
import time
import math

from pymavlink import mavutil


def collect(link, timeout):
    def reject_transmission(*_args, **_kwargs):
        raise PermissionError("passive GNSS audit forbids MAVLink transmission")
    link.write = reject_transmission
    heartbeat = link.wait_heartbeat(timeout=10)
    if heartbeat is None:
        raise SystemExit("no heartbeat")
    latest = {}
    texts = []
    deadline = time.monotonic() + timeout
    wanted = {
        "GPS_RAW_INT", "GPS2_RAW", "GPS_STATUS", "GLOBAL_POSITION_INT",
        "SCALED_PRESSURE", "SCALED_PRESSURE2", "SCALED_PRESSURE3",
    }
    while time.monotonic() < deadline:
        message = link.recv_match(blocking=True, timeout=0.5)
        if message is None or message.get_type() == "BAD_DATA":
            continue
        kind = message.get_type()
        if kind in wanted:
            values = message.to_dict()
            values.pop("mavpackettype", None)
            latest[kind] = values
        elif kind == "STATUSTEXT":
            text = message.text.rstrip("\x00")
            if text and text not in texts:
                texts.append(text)
    return {
        "schema": "efis-next.gnss-audit.v1",
        "capture_mode": "PASSIVE_RX_ONLY",
        "messages": latest,
        "status_text": texts,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", required=True)
    parser.add_argument("--timeout", type=float, default=25)
    args = parser.parse_args()
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be finite and positive")
    link = mavutil.mavlink_connection(args.device, baud=115200, source_system=254)
    try:
        result = collect(link, args.timeout)
        result["device"] = args.device
    finally:
        link.close()
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
