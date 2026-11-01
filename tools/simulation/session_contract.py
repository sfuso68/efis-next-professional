"""Offline SIL object validators. No network, FDM execution or actuator API.

Validation is not authentication, command application or runtime readiness.
Wire DDP/EDP and the R1-20 forced-command interface are not changed here.
"""
from datetime import datetime
import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = "fuso-avionics.sil-session.v1"


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def exact(obj, names):
    require(isinstance(obj, dict) and set(obj) == set(names.split()), "object fields")


def integer(n, low=0, high=2**53-1):
    require(type(n) is int and low <= n <= high, "integer range")


def number(n, low=-1e12, high=1e12):
    require(type(n) in (int, float) and math.isfinite(n) and low <= n <= high, "number range")


def token(s):
    require(isinstance(s, str) and re.fullmatch(r"[A-Za-z0-9_-]{1,64}", s), "identifier")


def digest(s):
    require(isinstance(s, str) and re.fullmatch(r"[a-f0-9]{64}", s), "sha256")


def pairs(items):
    obj = {}
    for key, value in items:
        require(key not in obj, "duplicate JSON key")
        obj[key] = value
    return obj


def decode(raw):
    require(isinstance(raw, (str, bytes)), "JSON input type")
    if isinstance(raw, bytes):
        raw = raw.decode("utf-8", errors="strict")
    require(len(raw.encode("utf-8")) <= 65536, "JSON size")
    def finite_float(text):
        value = float(text)
        require(math.isfinite(value), "nonfinite JSON number")
        return value
    try:
        return json.loads(raw, object_pairs_hook=pairs, parse_float=finite_float,
                          parse_constant=lambda _: (_ for _ in ()).throw(ValueError("nonfinite JSON")))
    except (RecursionError, UnicodeError) as exc:
        raise ValueError("JSON nesting or encoding") from exc


ICD = decode((ROOT / "config/protocols/sil-session-icd-v1.json").read_bytes())


def envelope(obj, kind):
    require(obj["schema"] == SCHEMA and obj["kind"] == kind, "schema/kind")
    require(obj["simulated"] is True and obj["flight_approval"] is False, "SIM boundary")
    token(obj["session_id"])
    integer(obj["epoch"])


def manifest(obj):
    exact(obj, "schema kind session_id epoch simulated flight_approval mode epoch_utc "
          "integration_hz output_hz source_node config_sha256 model_sha256 source_commit "
          "profile_status event_ids station_ids route_hashes")
    envelope(obj, "MANIFEST")
    require(obj["mode"] in ("LIVE", "OFFLINE"), "run mode")
    require(obj["profile_status"] in ("SYNTHETIC_FIXTURE", "REFERENCE_UNVALIDATED"), "model approval not supported")
    for name, expected in (("integration_hz", 100), ("output_hz", 20), ("source_node", 65000)):
        integer(obj[name])
        require(obj[name] == expected, "pinned SIL parameter")
    for name in ("config_sha256", "model_sha256"):
        digest(obj[name])
    require(isinstance(obj["source_commit"], str) and re.fullmatch(r"[a-f0-9]{40}", obj["source_commit"]), "source commit")
    require(isinstance(obj["epoch_utc"], str) and re.fullmatch(
        r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,6})?Z", obj["epoch_utc"]), "UTC required")
    datetime.fromisoformat(obj["epoch_utc"][:-1] + "+00:00")
    for name in ("event_ids", "station_ids", "route_hashes"):
        values = obj[name]
        require(isinstance(values, list) and len(values) <= 64, "catalogue bound")
        for value in values:
            (digest if name == "route_hashes" else token)(value)
        require(len(values) == len(set(values)), "duplicate catalogue identity")
    return obj


def sample(obj, unit, tick):
    exact(obj, "value raw unit validity simulated source_node channel sample_tick age_ms "
          "quality calibration_revision profile_revision")
    require(obj["unit"] == unit and obj["validity"] in ICD["sample_validities"], "unit/validity")
    require(obj["simulated"] is True and obj["source_node"] == 65000, "sample origin")
    integer(obj["source_node"])
    integer(obj["channel"], 0, 65535)
    integer(obj["sample_tick"], 0, tick)
    integer(obj["age_ms"], (tick-obj["sample_tick"])*10)
    number(obj["quality"], 0, 1)
    token(obj["calibration_revision"])
    token(obj["profile_revision"])
    if obj["validity"] in ("UNAVAILABLE", "INVALID"):
        require(obj["value"] is None and obj["raw"] is None, "missing data must be null")
    else:
        for value in (obj["value"], obj["raw"]):
            if unit == "bool":
                require(type(value) is bool, "boolean measure")
            else:
                number(value)


def frame(obj, context):
    manifest(context)
    exact(obj, "schema kind session_id epoch simulated flight_approval sequence tick sim_time_ns "
          "producer_mono_ns config_sha256 groups products")
    envelope(obj, "FRAME")
    require((obj["session_id"], obj["epoch"], obj["config_sha256"]) ==
            (context["session_id"], context["epoch"], context["config_sha256"]), "frame binding")
    integer(obj["sequence"])
    integer(obj["tick"])
    integer(obj["sim_time_ns"])
    integer(obj["producer_mono_ns"])
    require(obj["tick"] == obj["sequence"] * 5 and obj["sim_time_ns"] == obj["tick"] * 10000000, "fixed step clock")
    exact(obj["groups"], "flight engine airframe")
    for group, signals in ICD["signals"].items():
        require(isinstance(obj["groups"][group], dict) and set(obj["groups"][group]) == set(signals), "signal coverage")
        for name, unit in signals.items():
            sample(obj["groups"][group][name], unit, obj["tick"])
    products = obj["products"]
    require(isinstance(products, dict) and set(products) == set(ICD["product_kinds"]), "product coverage")
    for product in products.values():
        exact(product, "status sha256 observed_sim_ns valid_until_sim_ns")
        require(product["status"] in ("SIMULATED", "UNAVAILABLE", "STALE", "INVALID"), "product status")
        if product["status"] in ("UNAVAILABLE", "INVALID"):
            require(all(product[k] is None for k in ("sha256", "observed_sim_ns", "valid_until_sim_ns")), "unavailable product")
        else:
            digest(product["sha256"])
            integer(product["observed_sim_ns"], 0, obj["sim_time_ns"])
            integer(product["valid_until_sim_ns"], product["observed_sim_ns"])
            if product["status"] == "SIMULATED":
                require(product["valid_until_sim_ns"] >= obj["sim_time_ns"], "expired product")
    return obj


def command(obj, context, *, current_tick, authority_id, lease_id, lease_until_tick):
    """Stateless preflight only: caller must authenticate and implement queue/ack."""
    manifest(context)
    exact(obj, "schema kind session_id epoch simulated flight_approval command_id sequence "
          "authority_id lease_id apply_tick expires_tick action args")
    envelope(obj, "COMMAND")
    require((obj["session_id"], obj["epoch"]) == (context["session_id"], context["epoch"]), "command binding")
    for name in ("command_id", "authority_id", "lease_id"):
        token(obj[name])
    integer(current_tick)
    integer(lease_until_tick, current_tick)
    require(obj["authority_id"] == authority_id and obj["lease_id"] == lease_id, "command lease")
    integer(obj["sequence"], 1)
    integer(obj["apply_tick"], current_tick+1, min(current_tick+100, lease_until_tick))
    integer(obj["expires_tick"], obj["apply_tick"], min(current_tick+100, lease_until_tick))
    action, args = obj["action"], obj["args"]
    require(action in ICD["commands"], "unknown action")
    if action in ("START", "STOP", "PAUSE", "RESUME"):
        exact(args, "")
    elif action == "SET_CONTROL":
        exact(args, "control value")
        require(isinstance(args["control"], str) and args["control"] in ICD["controls"], "unknown control")
        number(args["value"], *ICD["controls"][args["control"]])
    elif action == "SET_OBS":
        exact(args, "degrees_magnetic")
        number(args["degrees_magnetic"], 0, 359.999999)
    else:
        field, catalogue = {"SELECT_NAV": ("station_id", "station_ids"),
                            "LOAD_ROUTE": ("sha256", "route_hashes"),
                            "SCENARIO_EVENT": ("event_id", "event_ids")}[action]
        exact(args, field)
        require(isinstance(args[field], str) and args[field] in context[catalogue], "object not frozen in manifest")
    return {"validation": "PASS", "application": "NOT_APPLIED", "command_id": obj["command_id"]}


def acknowledgement(obj, request):
    """Correlate a previously validated request; not proof physics applied it."""
    exact(obj, "schema kind session_id epoch simulated flight_approval command_id sequence "
          "status reason applied_tick")
    envelope(obj, "ACK")
    for key in ("session_id", "epoch", "command_id", "sequence"):
        require(obj[key] == request[key] and type(obj[key]) is type(request[key]), "ack binding")
    require(obj["status"] in ICD["ack_states"] and obj["reason"] in ICD["ack_reasons"], "ack enums")
    if obj["status"] == "APPLIED":
        require(obj["reason"] == "NONE", "applied reason")
        integer(obj["applied_tick"], request["apply_tick"], request["expires_tick"])
    else:
        require(obj["applied_tick"] is None, "unapplied tick")
        require((obj["reason"] == "NONE") == (obj["status"] == "QUEUED"), "ack reason")
    return obj
