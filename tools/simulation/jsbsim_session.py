"""Headless fixed-step JSBSim reference sessions, not a production wire publisher."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import time
import uuid

from tools.simulation.sil_world import World, config_digest, radar_png

SCHEMA = "fuso-avionics.physics-sil.v1"
VERSION = "1.3.3"
DT = 0.01
OUTPUT_HZ = 20
MAX_REALTIME_LATENESS_S = 0.1
SCENARIOS = ("cruise", "turn", "engine-out", "sensor-loss")
FLIGHT = {
    "latitude_deg": ("position/lat-geod-deg", "deg", 1),
    "longitude_deg": ("position/long-gc-deg", "deg", 1),
    "altitude_msl_ft": ("position/h-sl-ft", "ft", 1),
    "cas_kt": ("velocities/vc-kts", "kt", 1),
    "tas_kt": ("velocities/vtrue-kts", "kt", 1),
    "north_fps": ("velocities/v-north-fps", "ft/s", 1),
    "east_fps": ("velocities/v-east-fps", "ft/s", 1),
    "vs_fpm": ("velocities/v-down-fps", "ft/min", -60),
    "heading_true_deg": ("attitude/psi-deg", "deg", 1),
    "pitch_deg": ("attitude/theta-deg", "deg", 1),
    "roll_deg": ("attitude/phi-deg", "deg", 1),
}
ENGINE = {
    "rpm": ("propulsion/engine/engine-rpm", "rpm", 1),
    "map_inhg": ("propulsion/engine/map-inhg", "inHg", 1),
    "oil_pressure_psi": ("propulsion/engine/oil-pressure-psi", "psi", 1),
    "oil_temperature_c": ("propulsion/engine/oil-temperature-degF", "degC", None),
    "flow_gph": ("propulsion/engine/fuel-flow-rate-gph", "USgal/h", 1),
}
MISSING_ENGINE = {"fuel_pressure_psi": "psi", "bus_v": "V", "current_a": "A",
                  **{f"cht{i}": "degC" for i in range(1, 5)},
                  **{f"egt{i}": "degC" for i in range(1, 5)}}


def clip(value, low, high):
    if not math.isfinite(value):
        raise ValueError("nonfinite control")
    return max(low, min(value, high))


def sample(value, unit, prop, scale=1):
    if value is None or not math.isfinite(value):
        return {"value": None, "raw": None, "unit": unit, "property": prop,
                "validity": "UNAVAILABLE", "simulated": True}
    engineering = (value - 32) * 5 / 9 if scale is None else value * scale
    if not math.isfinite(engineering):
        raise ValueError("conversion overflow")
    return {"value": engineering, "raw": value, "unit": unit, "property": prop,
            "validity": "SIMULATED", "simulated": True}


def model_tree_digest(root):
    """Bind all XML aircraft/engine/system dependencies, not just the main model."""
    digest = hashlib.sha256()
    count = 0
    for directory in ("aircraft", "engine", "systems"):
        for path in sorted((root / directory).rglob("*.xml")):
            relative = path.relative_to(root).as_posix()
            digest.update(relative.encode("utf-8") + b"\0")
            digest.update(hashlib.sha256(path.read_bytes()).digest())
            count += 1
    if not count:
        raise ValueError("no model dependencies")
    return {"sha256": digest.hexdigest(), "xml_files": count}


class Backend:
    def __init__(self, world_config=None):
        import jsbsim  # isolated runtime dependency, never imported by offline unit tests
        if jsbsim.__version__ != VERSION:
            raise ValueError("JSBSim version mismatch")
        self.root = Path(jsbsim.__file__).resolve().parent
        self.model_digest = model_tree_digest(self.root)
        self.f = jsbsim.FGFDMExec(str(self.root))
        self.f.set_debug_level(0)
        self.f.set_output_path("/tmp" if os.name != "nt" else os.environ.get("TEMP", "."))
        if not self.f.load_model("c172p"):
            raise ValueError("C172P model load failed")
        self.f.disable_output()
        self.catalog = {s.rsplit(" (", 1)[0] for s in self.f.get_property_catalog()}
        self.f.set_dt(DT)
        self.world = World(world_config) if world_config is not None else None
        # Set position before velocity, because JSBSim initial-condition setters interact.
        for key, value in (("ic/lat-geod-deg", 45.648399), ("ic/long-gc-deg", 12.1944),
                           ("ic/h-sl-ft", 5000), ("ic/psi-true-deg", 5),
                           ("ic/gamma-deg", 0), ("ic/vc-kts", 90),
                           ("propulsion/set-running", -1)):
            self.f[key] = value
        if self.world is not None:
            self.world.apply_fdm(self, 0)
        if not self.f.run_ic():
            raise ValueError("initial conditions failed")
        self.f.do_trim(0)
        self.base_e = self.f["fcs/elevator-cmd-norm"]
        self.base_t = self.f["fcs/throttle-cmd-norm"]
        self.base_p = self.f["attitude/theta-deg"]
        self.base_v = self.f["velocities/vc-kts"]
        self.initial = {"altitude_ft": self.f["position/h-sl-ft"],
                        "cas_kt": self.base_v, "pitch_deg": self.base_p,
                        "throttle": self.base_t}
        self.ticks = 0
        self.test_controls = {}

    def set_test_control(self, control, value):
        from tools.simulation.pilot_fixture import INPUTS
        if control not in INPUTS or INPUTS[control] not in self.catalog:
            raise ValueError("model control capability unavailable")
        from tools.simulation import session_contract as c
        c.number(value, *c.ICD["controls"][control])
        self.test_controls[control] = value

    def clear_test_controls(self):
        self.test_controls.clear()

    def read(self, prop):
        # A missing property is never auto-created as a plausible zero.
        if prop not in self.catalog:
            return None
        result = self.f[prop]
        return result if math.isfinite(result) else None

    def step(self, scenario):
        if scenario not in SCENARIOS:
            raise ValueError("unknown scenario")
        t = self.ticks * DT
        if getattr(self, "world", None) is not None:
            self.world.apply_fdm(self, self.ticks * 10)
        failed = scenario == "engine-out" and t >= 20
        target_roll = 20 if scenario == "turn" and 20 <= t < 40 else 0
        # Bench-only pilot. These are FDM inputs, NOT position/attitude setters.
        if failed:
            self.f["fcs/mixture-cmd-norm"] = 0
            desired_pitch = self.base_p + clip((self.f["velocities/vc-kts"] - self.base_v) * .25, -8, 5)
        else:
            vs = -self.f["velocities/v-down-fps"] * 60
            desired_pitch = self.base_p + clip((5000-self.f["position/h-sl-ft"])*.01-vs*.005, -8, 8)
        self.f["fcs/elevator-cmd-norm"] = clip(self.base_e + .05*(self.f["attitude/theta-deg"]-desired_pitch)
                                              + .2*self.f["velocities/q-rad_sec"], -1, 1)
        self.f["fcs/aileron-cmd-norm"] = clip(.04*(target_roll-self.f["attitude/phi-deg"])
                                             - .2*self.f["velocities/p-rad_sec"], -1, 1)
        self.f["fcs/throttle-cmd-norm"] = clip(self.base_t + .02*(self.base_v-self.f["velocities/vc-kts"]), 0, 1)
        # Explicit opt-in test commands override only these FDM inputs. The
        # airborne bench pilot writes all three again when the lease releases.
        from tools.simulation.pilot_fixture import INPUTS
        for control, value in getattr(self, "test_controls", {}).items():
            self.f[INPUTS[control]] = value
        if not self.f.run():
            raise ValueError("FDM stopped")
        self.ticks += 1
        if abs(self.f.get_sim_time() - self.ticks * DT) > 1e-5:
            raise ValueError("FDM clock drift")
        if self.f["position/h-sl-ft"] < 100:
            raise ValueError("reference airborne scenario left altitude envelope")

    def frame(self, scenario, session_id):
        t_ms = self.ticks * 10
        flight = {name: sample(self.read(prop), unit, prop, scale)
                  for name, (prop, unit, scale) in FLIGHT.items()}
        if any(s["value"] is None for s in flight.values()):
            raise ValueError("required flight property unavailable")
        north, east = flight["north_fps"]["value"], flight["east_fps"]["value"]
        flight["gs_kt"] = sample(math.hypot(north, east) / 1.687809857, "kt", "derived:north/east")
        if scenario == "sensor-loss" and 20000 <= t_ms < 40000:
            flight["cas_kt"] = sample(None, "kt", "fault:CAS_unavailable")
        engine = {name: sample(self.read(prop), unit, prop, scale)
                  for name, (prop, unit, scale) in ENGINE.items()}
        for name, unit in MISSING_ENGINE.items():
            engine[name] = sample(None, unit, "NOT_MODELED_PER_CHANNEL")
        for i, name in enumerate(("left_gal", "right_gal")):
            tank = "propulsion/tank" if i == 0 else "propulsion/tank[1]"
            mass, density = self.read(tank+"/contents-lbs"), self.read(tank+"/density-lbs_per_gal")
            value = mass / density if mass is not None and density is not None and density > 0 else None
            engine[name] = sample(value, "USgal", tank+":mass/density")
        result = {"schema": SCHEMA, "session_id": session_id, "sequence": self.ticks // 5,
                "sim_time_ms": t_ms, "simulated": True, "flight_approval": False,
                "backend": "JSBSim-"+VERSION, "model": "c172p", "profile_status": "REFERENCE_UNVALIDATED",
                "scenario": scenario, "flight": flight, "engine": engine,
                "engine_aggregate": {
                    "cht_mean_c": sample(self.read("propulsion/engine/cht-degF"), "degC", "propulsion/engine/cht-degF", None),
                    "egt_mean_c": sample(self.read("propulsion/engine/egt-degF"), "degC", "propulsion/engine/egt-degF", None)},
                "airframe": {"flap_measured_deg": sample(self.read("fcs/flap-pos-deg"), "deg", "fcs/flap-pos-deg"),
                             "gear_lock_feedback": sample(None, "bool", "FIXED_GEAR_NO_LOCK_SENSOR")},
                "events": ["ENGINE_MIXTURE_CUTOFF"] if scenario == "engine-out" and t_ms >= 20000 else [],
                "conventions": {"heading": "TRUE_NOT_MAGNETIC", "altitude": "GEOMETRIC_MSL_NOT_QNH_INDICATED",
                                "airspeed": "CALIBRATED_NOT_RAW_PITOT", "clock": "INTEGER_SIMULATION_MS"}}
        if getattr(self, "world", None) is not None:
            actual = {"pressure_hpa": self.read("atmosphere/P-psf"),
                      "temperature_c": self.read("atmosphere/T-R"),
                      "density_kg_m3": self.read("atmosphere/rho-slugs_ft3")}
            if any(v is None for v in actual.values()):
                raise ValueError("FDM atmosphere output missing")
            actual = {"pressure_hpa": actual["pressure_hpa"]*47.88025898/100,
                      "temperature_c": actual["temperature_c"]*5/9-273.15,
                      "density_kg_m3": actual["density_kg_m3"]*515.378818}
            result["world"] = self.world.frame(flight, t_ms, actual)
        return result


def write_json(path, value):
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write("\n")


class RealtimePacer:
    """Fixed monotonic deadlines; never silently fast-forward after a stall."""
    def __init__(self, clock=None, sleep=None):
        self.clock = clock or time.monotonic
        self.sleep = sleep or time.sleep
        self.start = self.clock()
        if not math.isfinite(self.start):
            raise ValueError("nonfinite monotonic clock")
        self.maximum_lateness_s = 0.0
        self.last_tick = -1

    def wait(self, tick):
        if type(tick) is not int or tick != self.last_tick + 1:
            raise ValueError("realtime tick discontinuity")
        deadline = self.start + tick * DT
        now = self.clock()
        if not math.isfinite(now):
            raise ValueError("nonfinite monotonic clock")
        while now < deadline:
            self.sleep(deadline - now)
            now = self.clock()
            if not math.isfinite(now):
                raise ValueError("nonfinite monotonic clock")
        lateness = now - deadline
        self.maximum_lateness_s = max(self.maximum_lateness_s, lateness)
        if lateness > MAX_REALTIME_LATENESS_S:
            raise ValueError("realtime deadline exceeded; stop instead of catchup")
        self.last_tick = tick

    def report(self):
        return {"mode": "MONOTONIC_FIXED_STEP", "ticks": self.last_tick + 1,
                "maximum_lateness_ms": self.maximum_lateness_s * 1000,
                "lateness_limit_ms": MAX_REALTIME_LATENESS_S * 1000,
                "elapsed_seconds": self.clock() - self.start,
                "overrun_policy": "STOP_NO_CATCHUP"}


def run_session(directory, scenario="cruise", seconds=60, session_id=None, realtime=False, world_config=None,
                exchange=None):
    if scenario not in SCENARIOS or not isinstance(seconds, int) or not 1 <= seconds <= 3600:
        raise ValueError("scenario/duration invalid")
    session_id = session_id or uuid.uuid4().hex
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", session_id):
        raise ValueError("session identifier invalid")
    if world_config is not None:
        config_digest(world_config)  # validate before creating output
    if exchange is not None and (not realtime or world_config is None):
        raise ValueError("live exchange requires realtime world")
    directory = Path(directory)
    directory.mkdir(parents=False, exist_ok=False)  # never replace a prior session
    try:
        backend = Backend(world_config) if world_config is not None else Backend()
        if exchange is not None:
            exchange.start(session_id, seconds)
        pacer = None
        frames = 0
        digest = hashlib.sha256()
        with (directory / "frames.jsonl").open("xb") as stream:
            for tick in range(seconds * 100 + 1):
                if exchange is not None and tick % 5 == 0:
                    exchange.request(tick // 5)
                if realtime and pacer is None:
                    pacer = RealtimePacer()
                if pacer:
                    pacer.wait(tick)
                if tick:
                    backend.step(scenario)
                if tick % 5 == 0:
                    frame = backend.frame(scenario, session_id)
                    line = (json.dumps(frame, sort_keys=True, separators=(",", ":"), allow_nan=False)+"\n").encode()
                    stream.write(line)
                    digest.update(line)
                    frames += 1
                    if exchange is not None:
                        exchange.send(frame)
            stream.flush()
            os.fsync(stream.fileno())
        manifest = {"schema": SCHEMA, "status": "COMPLETE_OFFLINE_REFERENCE",
                    "session_id": session_id, "simulated": True, "flight_approval": False,
                    "backend": "JSBSim-"+VERSION, "model": "c172p", "scenario": scenario,
                    "profile_status": "REFERENCE_UNVALIDATED", "duration_seconds": seconds,
                    "integration_hz": 100, "output_hz": OUTPUT_HZ, "frames": frames,
                    "frames_sha256": digest.hexdigest(), "model_tree": backend.model_digest,
                    "initial_trim": backend.initial, "network_publisher": False,
                    "efis_nodes_connected": False}
        if pacer:
            # Timing evidence, not evidence of a connected display or hard real time.
            manifest["realtime_timing"] = pacer.report()
        if exchange is not None:
            # Only the separate receiver can attest to a connected display.
            manifest["efis_nodes_connected"] = None
            manifest["live_exchange"] = {"protocol": "fuso-avionics.sil-live.v1",
                                         "requests": frames, "transport": "STDIO_CHALLENGE",
                                         "display_acceptance": "NOT_IMPLIED"}
        if world_config is not None:
            manifest["world_config"] = world_config
            manifest["world_config_sha256"] = config_digest(world_config)
            # Distinct SIM-only image and georeferencing sidecar; never operational cache.
            radar = backend.world.radar(0)
            image = radar_png(radar)
            with (directory / "sim-radar.png").open("xb") as stream:
                stream.write(image)
            write_json(directory / "sim-radar.json", {k: v for k, v in radar.items() if k != "values"})
            manifest["radar_png_sha256"] = hashlib.sha256(image).hexdigest()
            manifest["radar_metadata_sha256"] = hashlib.sha256((directory / "sim-radar.json").read_bytes()).hexdigest()
        write_json(directory / "manifest.json", manifest)
        return manifest
    except BaseException as exc:
        write_json(directory / "failure.json", {"status": "FAILED", "simulated": True,
                   "flight_approval": False, "error_type": type(exc).__name__, "reason": str(exc)})
        raise


def strict_json(text):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON key")
            result[key] = value
        return result
    def invalid_constant(_):
        raise ValueError("nonfinite JSON")
    return json.loads(text, object_pairs_hook=unique, parse_constant=invalid_constant)


def verify_session(directory):
    directory = Path(directory)
    text = (directory / "manifest.json").read_text(encoding="utf-8")
    if len(text) > 65536:
        raise ValueError("oversize manifest")
    manifest = strict_json(text)
    if not isinstance(manifest, dict):
        raise ValueError("manifest not object")
    if ((directory / "failure.json").exists() or manifest.get("schema") != SCHEMA
            or manifest.get("status") != "COMPLETE_OFFLINE_REFERENCE"
            or manifest.get("simulated") is not True or manifest.get("flight_approval") is not False):
        raise ValueError("session not complete SIM")
    if (type(manifest.get("duration_seconds")) is not int or not 1 <= manifest["duration_seconds"] <= 3600
            or type(manifest.get("frames")) is not int
            or manifest.get("integration_hz") != 100 or manifest.get("output_hz") != OUTPUT_HZ
            or manifest.get("model") != "c172p" or manifest.get("backend") != "JSBSim-"+VERSION
            or manifest.get("profile_status") != "REFERENCE_UNVALIDATED"
            or not isinstance(manifest.get("session_id"), str)
            or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", manifest["session_id"])):
        raise ValueError("manifest contract")
    digest, count = hashlib.sha256(), 0
    if "realtime_timing" in manifest:
        timing = manifest["realtime_timing"]
        if (not isinstance(timing, dict) or timing.get("mode") != "MONOTONIC_FIXED_STEP"
                or type(timing.get("ticks")) is not int
                or timing["ticks"] != manifest["duration_seconds"] * 100 + 1
                or timing.get("overrun_policy") != "STOP_NO_CATCHUP"
                or timing.get("lateness_limit_ms") != MAX_REALTIME_LATENESS_S * 1000):
            raise ValueError("realtime timing contract")
        for key in ("maximum_lateness_ms", "elapsed_seconds"):
            if type(timing.get(key)) not in (int, float) or not math.isfinite(timing[key]):
                raise ValueError("nonfinite realtime timing")
        if (not 0 <= timing["maximum_lateness_ms"] <= timing["lateness_limit_ms"]
                or timing["elapsed_seconds"] < manifest["duration_seconds"]):
            raise ValueError("realtime timing acceptance")
    world = None
    if "world_config" in manifest:
        if config_digest(manifest["world_config"]) != manifest.get("world_config_sha256"):
            raise ValueError("world configuration digest")
        world = World(manifest["world_config"])
        for filename, key in (("sim-radar.png", "radar_png_sha256"), ("sim-radar.json", "radar_metadata_sha256")):
            path = directory / filename
            if path.is_symlink() or not path.is_file() or path.stat().st_size > 1048576 or hashlib.sha256(path.read_bytes()).hexdigest() != manifest.get(key):
                raise ValueError("world radar artifact digest")
    with (directory / "frames.jsonl").open("rb") as stream:
        while True:
            line = stream.readline(65537)
            if not line:
                break
            if len(line) > 65536:
                raise ValueError("oversize frame")
            digest.update(line)
            frame = strict_json(line)
            if not isinstance(frame, dict):
                raise ValueError("frame not object")
            if (frame.get("schema") != SCHEMA or frame.get("session_id") != manifest["session_id"]
                    or frame.get("sequence") != count or frame.get("sim_time_ms") != count * 50
                    or type(frame.get("sequence")) is not int or type(frame.get("sim_time_ms")) is not int
                    or frame.get("simulated") is not True or frame.get("flight_approval") is not False):
                raise ValueError("frame clock/identity/SIM mismatch")
            for group in ("flight", "engine", "engine_aggregate", "airframe"):
                if not isinstance(frame.get(group), dict):
                    raise ValueError("sample group missing")
                if group == "flight" and set(frame[group]) != set(FLIGHT) | {"gs_kt"}:
                    raise ValueError("flight fields mismatch")
                if group == "engine" and set(frame[group]) != set(ENGINE) | set(MISSING_ENGINE) | {"left_gal", "right_gal"}:
                    raise ValueError("engine fields mismatch")
                for datum in frame[group].values():
                    if not isinstance(datum, dict) or datum.get("simulated") is not True:
                        raise ValueError("sample origin mismatch")
                    if (not isinstance(datum.get("unit"), str) or not datum["unit"]
                            or not isinstance(datum.get("property"), str) or not datum["property"]):
                        raise ValueError("sample provenance missing")
                    value = datum.get("value")
                    if datum.get("validity") == "SIMULATED":
                        if type(value) not in (int, float) or not math.isfinite(value):
                            raise ValueError("nonfinite sample")
                        raw = datum.get("raw")
                        if type(raw) not in (int, float) or not math.isfinite(raw):
                            raise ValueError("nonfinite raw sample")
                    elif datum.get("validity") != "UNAVAILABLE" or value is not None:
                        raise ValueError("unavailable sample not masked")
            if world is not None:
                value = frame.get("world")
                if not isinstance(value, dict) or not isinstance(value.get("fdm_atmosphere"), dict) or value != world.frame(frame["flight"], frame["sim_time_ms"], value["fdm_atmosphere"]):
                    raise ValueError("world navigation/weather replay mismatch")
            elif "world" in frame:
                raise ValueError("world frame without bound configuration")
            count += 1
            if count > 72001:
                raise ValueError("too many frames")
    if (count != manifest["frames"] or count != manifest["duration_seconds"] * 20 + 1
            or digest.hexdigest() != manifest["frames_sha256"]):
        raise ValueError("session digest/count mismatch")
    return manifest


def role_view(frame, role):
    """Offline test views share a clock; NOT the actual PFD/MFD/EMS consumer."""
    if role not in ("PFD", "MFD", "EMS"):
        raise ValueError("unknown consumer role")
    groups = ("engine", "engine_aggregate", "airframe") if role == "EMS" else ("flight", "engine", "airframe")
    return {"schema": SCHEMA, "session_id": frame["session_id"], "sequence": frame["sequence"],
            "sim_time_ms": frame["sim_time_ms"], "simulated": True, "flight_approval": False,
            "role": role, "offline_test_view": True, **{g: frame[g] for g in groups}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", required=True, type=Path)
    parser.add_argument("--scenario", choices=SCENARIOS, default="cruise")
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--session-id")
    parser.add_argument("--realtime", action="store_true")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--world", type=Path, help="Explicit offline SIM weather/navigation configuration")
    args = parser.parse_args()
    try:
        if args.world and (args.verify or args.world.stat().st_size > 65536):
            raise ValueError("world is bounded generation input only; replay uses bound manifest")
        config = strict_json(args.world.read_text(encoding="utf-8")) if args.world else None
        result = verify_session(args.directory) if args.verify else run_session(
            args.directory, args.scenario, args.seconds, args.session_id, args.realtime, config)
    except (ValueError, OSError, RuntimeError, ImportError) as exc:
        parser.exit(2, f"SIL: {exc}\n")
    print(json.dumps(result, sort_keys=True, allow_nan=False))


if __name__ == "__main__":
    main()
