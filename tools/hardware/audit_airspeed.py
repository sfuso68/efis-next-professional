#!/usr/bin/env python3
"""Read-only dynamic airspeed audit over MAVLink."""
import argparse
import json
import statistics
import time

from pymavlink import mavutil


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", required=True)
    parser.add_argument("--timeout", type=float, default=20)
    args = parser.parse_args()
    link = mavutil.mavlink_connection(args.device, baud=115200, source_system=254)
    if link.wait_heartbeat(timeout=10) is None:
        raise SystemExit("no heartbeat")
    link.mav.request_data_stream_send(link.target_system, link.target_component,
                                      mavutil.mavlink.MAV_DATA_STREAM_ALL, 10, 1)
    samples = {"airspeed_m_s": [], "press_diff_pa": [], "temperature_c": []}
    latest = {}
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        message = link.recv_match(blocking=True, timeout=0.5)
        if message is None or message.get_type() == "BAD_DATA":
            continue
        kind = message.get_type()
        values = message.to_dict()
        values.pop("mavpackettype", None)
        if kind == "AIRSPEED":
            latest[kind] = values
            for key, field in (("airspeed_m_s", "airspeed"),
                               ("press_diff_pa", "press_diff"),
                               ("temperature_c", "temperature")):
                if field in values:
                    samples[key].append(float(values[field]))
        elif kind == "VFR_HUD":
            latest[kind] = values
            samples["airspeed_m_s"].append(float(values["airspeed"]))
    summary = {}
    for key, values in samples.items():
        if values:
            summary[key] = {"count": len(values), "min": min(values),
                            "max": max(values), "mean": statistics.fmean(values)}
    print(json.dumps({"schema": "efis-next.airspeed-audit.v1", "messages": latest,
                      "summary": summary}, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
