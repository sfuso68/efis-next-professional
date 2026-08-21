#!/usr/bin/env python3
"""Read-only GNSS message audit over MAVLink."""
import argparse
import json
import time

from pymavlink import mavutil


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", required=True)
    parser.add_argument("--timeout", type=float, default=25)
    args = parser.parse_args()
    link = mavutil.mavlink_connection(args.device, baud=115200, source_system=254)
    heartbeat = link.wait_heartbeat(timeout=10)
    if heartbeat is None:
        raise SystemExit("no heartbeat")
    link.mav.request_data_stream_send(
        link.target_system, link.target_component,
        mavutil.mavlink.MAV_DATA_STREAM_ALL, 4, 1,
    )
    latest = {}
    texts = []
    deadline = time.monotonic() + args.timeout
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
    print(json.dumps({
        "schema": "efis-next.gnss-audit.v1",
        "device": args.device,
        "messages": latest,
        "status_text": texts,
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
