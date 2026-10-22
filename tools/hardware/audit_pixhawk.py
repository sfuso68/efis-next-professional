#!/usr/bin/env python3
"""Read-only MAVLink identity and health audit for a connected autopilot."""
import argparse
import json
import time
import math

from pymavlink import mavutil


def version_string(value):
    return f"{(value >> 24) & 0xff}.{(value >> 16) & 0xff}.{(value >> 8) & 0xff}.{value & 0xff}"


def enum_name(group, value):
    item = mavutil.mavlink.enums.get(group, {}).get(value)
    return item.name if item else str(value)


def collect(connection, timeout):
    """Observe unsolicited traffic only; never request version or streams."""
    def reject_transmission(*_args, **_kwargs):
        raise PermissionError("passive audit forbids MAVLink transmission")

    # pymavlink serializers write through their connection object. Fail closed
    # if a future receive-side helper attempts an implicit protocol response.
    connection.write = reject_transmission
    heartbeat = connection.wait_heartbeat(timeout=timeout)
    if heartbeat is None:
        raise RuntimeError("no MAVLink heartbeat received")

    result = {
        "schema": "efis-next.pixhawk-audit.v1",
        "capture_mode": "PASSIVE_RX_ONLY",
        "version_status": "NOT_OBSERVED",
        "target_system": connection.target_system,
        "target_component": connection.target_component,
        "heartbeat": {
            "vehicle_type": enum_name("MAV_TYPE", heartbeat.type),
            "autopilot": enum_name("MAV_AUTOPILOT", heartbeat.autopilot),
            "base_mode": heartbeat.base_mode,
            "custom_mode": heartbeat.custom_mode,
            "system_status": enum_name("MAV_STATE", heartbeat.system_status),
        },
        "status_text": [],
    }
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        message = connection.recv_match(blocking=True, timeout=0.5)
        if message is None or message.get_type() == "BAD_DATA":
            continue
        if message.get_type() == "AUTOPILOT_VERSION":
            result["version_status"] = "OBSERVED"
            result["version"] = {
                "flight": version_string(message.flight_sw_version),
                "middleware": version_string(message.middleware_sw_version),
                "os": version_string(message.os_sw_version),
                "board_version": message.board_version,
                "vendor_id": message.vendor_id,
                "product_id": message.product_id,
                "uid": str(message.uid),
                "uid2": bytes(message.uid2).hex(),
                "capabilities": int(message.capabilities),
                "flight_custom_version": bytes(message.flight_custom_version).hex(),
            }
        elif message.get_type() == "STATUSTEXT":
            text = message.text.rstrip("\x00")
            if text and text not in result["status_text"]:
                result["status_text"].append(text)
        if "version" in result and len(result["status_text"]) >= 2:
            break
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=12)
    args = parser.parse_args()

    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be finite and positive")

    connection = mavutil.mavlink_connection(args.device, baud=args.baud, source_system=254)
    try:
        result = collect(connection, args.timeout)
        result["device"] = args.device
    finally:
        connection.close()
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
