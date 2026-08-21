#!/usr/bin/env python3
"""Set one MAVLink parameter with an expected-old-value guard and readback."""
import argparse
import json
import math

from pymavlink import mavutil


def read_parameter(link, name, timeout=8):
    encoded = name.encode("ascii")
    link.mav.param_request_read_send(link.target_system, link.target_component, encoded, -1)
    message = link.recv_match(type="PARAM_VALUE", blocking=True, timeout=timeout,
                              condition=f'PARAM_VALUE.param_id=="{name}"')
    if message is None:
        raise RuntimeError(f"no value returned for {name}")
    return float(message.param_value), int(message.param_type)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--expected", type=float, required=True)
    parser.add_argument("--value", type=float, required=True)
    args = parser.parse_args()
    link = mavutil.mavlink_connection(args.device, baud=115200, source_system=254)
    if link.wait_heartbeat(timeout=10) is None:
        raise SystemExit("no heartbeat")
    before, parameter_type = read_parameter(link, args.name)
    if not math.isclose(before, args.expected, abs_tol=0.0001):
        raise SystemExit(f"guard failed: {args.name} is {before}, expected {args.expected}")
    link.mav.param_set_send(link.target_system, link.target_component,
                            args.name.encode("ascii"), args.value, parameter_type)
    acknowledgement = link.recv_match(type="PARAM_VALUE", blocking=True, timeout=8,
                                      condition=f'PARAM_VALUE.param_id=="{args.name}"')
    if acknowledgement is None or not math.isclose(float(acknowledgement.param_value), args.value, abs_tol=0.0001):
        raise SystemExit(f"write acknowledgement failed for {args.name}")
    after, _ = read_parameter(link, args.name)
    if not math.isclose(after, args.value, abs_tol=0.0001):
        raise SystemExit(f"readback failed: {args.name} is {after}")
    print(json.dumps({"parameter": args.name, "before": before, "after": after,
                      "verified": True}, sort_keys=True))


if __name__ == "__main__":
    main()
