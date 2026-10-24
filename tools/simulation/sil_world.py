"""Deterministic offline weather/radio/navigation world. No network or RF output.

Analytic training models, not certified receivers or an operational database.
The physical truth, receiver output and delayed weather products are separate.
"""
from __future__ import annotations

import copy
from datetime import datetime, timedelta, timezone
import hashlib
import json
import math
import re
import struct
import zlib

SCHEMA = "fuso-avionics.sil-world.v1"
R_NM = 3440.065
FT_PER_NM = 6076.11548556
FAULTS = {"gnss_loss", "radio_loss", "radio_ident_mismatch", "wx_offline", "wx_delay", "wx_clock_skew"}


def number(value, low=-1e9, high=1e9):
    if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
        raise ValueError("world number out of bounds")
    return float(value)


def exact(obj, keys):
    if not isinstance(obj, dict) or set(obj) != set(keys.split()):
        raise ValueError("world object fields mismatch")


def point(p):
    number(p["lat"], -85, 85)
    number(p["lon"], -180, 180)


def wrap(angle):
    return (angle + 180) % 360 - 180


def geometry(a, b):
    """Spherical great-circle distance NM and initial TRUE bearing degrees."""
    point(a)
    point(b)
    p1, p2 = math.radians(a["lat"]), math.radians(b["lat"])
    dl = math.radians(wrap(b["lon"] - a["lon"]))
    h = math.sin((p2-p1)/2)**2 + math.cos(p1)*math.cos(p2)*math.sin(dl/2)**2
    distance = 2 * R_NM * math.asin(math.sqrt(max(0, min(1, h))))
    bearing = math.degrees(math.atan2(math.sin(dl)*math.cos(p2),
                       math.cos(p1)*math.sin(p2)-math.sin(p1)*math.cos(p2)*math.cos(dl))) % 360
    return distance, bearing


def leg_geometry(a, b, p):
    length, course = geometry(a, b)
    distance, bearing = geometry(a, p)
    delta = math.radians(wrap(bearing-course))
    xtk = math.asin(max(-1, min(1, math.sin(distance/R_NM)*math.sin(delta))))*R_NM
    along = math.atan2(math.sin(distance/R_NM)*math.cos(delta), math.cos(distance/R_NM))*R_NM
    return length, course, xtk, along


def datum(value, unit, reason="ANALYTIC_REFERENCE"):
    if value is not None and type(value) in (int, float):
        number(value)
    return {"value": value, "unit": unit, "validity": "SIMULATED" if value is not None else "UNAVAILABLE",
            "simulated": True, "reason": reason}


def validate(config):
    exact(config, "schema dataset_id epoch_utc seed atmosphere stations route receiver faults radar")
    if config["schema"] != SCHEMA or not isinstance(config["dataset_id"], str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", config["dataset_id"]):
        raise ValueError("world schema/dataset")
    if not isinstance(config["epoch_utc"], str):
        raise ValueError("world UTC type")
    epoch = datetime.fromisoformat(config["epoch_utc"].replace("Z", "+00:00"))
    if epoch.tzinfo is None or epoch.utcoffset() != timedelta(0):
        raise ValueError("world epoch must be UTC")
    if type(config["seed"]) is not int or not 0 <= config["seed"] <= 65535:
        raise ValueError("world seed")
    a = config["atmosphere"]
    exact(a, "qnh_hpa temperature_sl_c wind_layers gust_kt gust_period_s vertical_gust_kt visibility_m cloud_base_ft rain_rate_mmh")
    for key, lo, hi in (("qnh_hpa", 900, 1080), ("temperature_sl_c", -30, 45),
                        ("gust_kt", 0, 30), ("gust_period_s", 1, 600), ("vertical_gust_kt", 0, 10),
                        ("visibility_m", 50, 9999), ("cloud_base_ft", 0, 30000), ("rain_rate_mmh", 0, 150)):
        number(a[key], lo, hi)
    if not isinstance(a["wind_layers"], list) or not 2 <= len(a["wind_layers"]) <= 12:
        raise ValueError("wind layers")
    previous = -1
    for layer in a["wind_layers"]:
        exact(layer, "altitude_ft north_kt east_kt")
        height = number(layer["altitude_ft"], 0, 35000)
        if height <= previous:
            raise ValueError("unordered wind layers")
        previous = height
        number(layer["north_kt"], -150, 150)
        number(layer["east_kt"], -150, 150)
    ids = set()
    if not isinstance(config["stations"], list) or len(config["stations"]) > 32:
        raise ValueError("station limit")
    for station in config["stations"]:
        exact(station, "id kind lat lon elevation_ft frequency variation_east_deg range_nm course_true_deg gs_deg")
        if not isinstance(station["id"], str) or not re.fullmatch(r"SIM[A-Z0-9]{1,8}", station["id"]) or station["id"] in ids:
            raise ValueError("station identity must be unique SIM identifier")
        ids.add(station["id"])
        point(station)
        if station["kind"] not in ("VOR", "NDB", "ILS"):
            raise ValueError("radio kind")
        number(station["elevation_ft"], -1000, 15000)
        number(station["frequency"], 108, 117.95) if station["kind"] != "NDB" else number(station["frequency"], 190, 1750)
        number(station["variation_east_deg"], -30, 30)
        number(station["range_nm"], 1, 200)
        number(station["course_true_deg"], 0, 359.9999)
        number(station["gs_deg"], 2, 6)
    if not isinstance(config["route"], list) or not 2 <= len(config["route"]) <= 64:
        raise ValueError("route limit")
    route_ids = set()
    for waypoint in config["route"]:
        exact(waypoint, "id lat lon")
        point(waypoint)
        if not isinstance(waypoint["id"], str) or not re.fullmatch(r"[A-Z0-9_-]{1,16}", waypoint["id"]) or waypoint["id"] in route_ids:
            raise ValueError("route identifier")
        route_ids.add(waypoint["id"])
    for start, end in zip(config["route"], config["route"][1:]):
        if not .1 <= geometry(start, end)[0] <= 500:
            raise ValueError("degenerate/oversize route leg")
    r = config["receiver"]
    exact(r, "obs_deg variation_east_deg altimeter_hpa ias_bias_kt selected_id gnss_error_north_m gnss_error_east_m direct_to")
    for key, lo, hi in (("obs_deg", 0, 359.9999), ("variation_east_deg", -30, 30),
                        ("altimeter_hpa", 900, 1080), ("ias_bias_kt", -20, 20),
                        ("gnss_error_north_m", -100, 100), ("gnss_error_east_m", -100, 100)):
        number(r[key], lo, hi)
    if not isinstance(r["selected_id"], str) or r["selected_id"] not in ids or (r["direct_to"] is not None and (not isinstance(r["direct_to"], str) or r["direct_to"] not in route_ids)):
        raise ValueError("unknown selected station/waypoint")
    if not isinstance(config["faults"], list) or len(config["faults"]) > 32:
        raise ValueError("fault limit")
    for fault in config["faults"]:
        exact(fault, "kind start_ms end_ms target")
        if not isinstance(fault["kind"], str) or fault["kind"] not in FAULTS or type(fault["start_ms"]) is not int or type(fault["end_ms"]) is not int:
            raise ValueError("fault type/time")
        if not 0 <= fault["start_ms"] < fault["end_ms"] <= 3600000:
            raise ValueError("fault window")
        if not isinstance(fault["target"], str) or (fault["target"] != "ALL" and (fault["kind"] not in ("radio_loss", "radio_ident_mismatch") or fault["target"] not in ids)):
            raise ValueError("fault target")
    radar = config["radar"]
    exact(radar, "bbox cell_lat cell_lon velocity_north_kt velocity_east_kt radius_nm peak_dbz")
    if not isinstance(radar["bbox"], list) or len(radar["bbox"]) != 4:
        raise ValueError("radar bbox")
    west, south, east, north = radar["bbox"]
    number(west, -180, 180); number(east, -180, 180)
    number(south, -85, 85); number(north, -85, 85)
    if not 0 < east-west <= 10 or not 0 < north-south <= 10:
        raise ValueError("radar span")
    point({"lat": radar["cell_lat"], "lon": radar["cell_lon"]})
    for key in ("velocity_north_kt", "velocity_east_kt"):
        number(radar[key], -100, 100)
    number(radar["radius_nm"], .1, 50); number(radar["peak_dbz"], 1, 65)
    return copy.deepcopy(config)


class World:
    def __init__(self, config):
        self.config = validate(config)
        self.epoch = datetime.fromisoformat(config["epoch_utc"].replace("Z", "+00:00"))
        self.active_leg = 0
        self.last_ms = -1
        self.direct_origin = None

    def fault(self, kind, ms, target="ALL"):
        return any(f["kind"] == kind and f["start_ms"] <= ms < f["end_ms"]
                   and f["target"] in ("ALL", target) for f in self.config["faults"])

    def atmosphere(self, altitude_ft, ms):
        """Dry troposphere analytic reference, geometric height approximated as geopotential."""
        h = number(altitude_ft, -1000, 30000)*.3048
        number(ms, 0, 3600000)
        a = self.config["atmosphere"]
        temp0 = a["temperature_sl_c"]+273.15
        temp = temp0-.0065*h
        pressure = a["qnh_hpa"]*(temp/temp0)**(9.80665/(287.05287*.0065))
        layers = a["wind_layers"]
        lower, upper = layers[0], layers[-1]
        if altitude_ft <= lower["altitude_ft"]:
            upper = lower
        elif altitude_ft >= upper["altitude_ft"]:
            lower = upper
        else:
            lower, upper = next((l, u) for l, u in zip(layers, layers[1:])
                                if l["altitude_ft"] <= altitude_ft <= u["altitude_ft"])
        f = 0 if lower is upper else (altitude_ft-lower["altitude_ft"])/(upper["altitude_ft"]-lower["altitude_ft"])
        phase = 2*math.pi*(ms/1000/a["gust_period_s"] + self.config["seed"]/65536)
        wind_n = lower["north_kt"]+f*(upper["north_kt"]-lower["north_kt"])+a["gust_kt"]*math.sin(phase)
        wind_e = lower["east_kt"]+f*(upper["east_kt"]-lower["east_kt"])+a["gust_kt"]*.5*math.sin(phase*2)
        return {"pressure_hpa": pressure, "temperature_c": temp-273.15,
                "density_kg_m3": pressure*100/(287.05287*temp), "qnh_hpa": a["qnh_hpa"],
                "wind_north_kt": wind_n, "wind_east_kt": wind_e,
                "wind_down_kt": a["vertical_gust_kt"]*math.sin(phase*3),
                "visibility_m": a["visibility_m"], "cloud_base_ft": a["cloud_base_ft"],
                "rain_rate_mmh": a["rain_rate_mmh"], "simulated": True,
                "model": "DRY_TROPOSPHERE_HARMONIC_GUST_REFERENCE", "icing": "NOT_MODELED"}

    def apply_fdm(self, backend, ms):
        """Only explicit atmospheric inputs, never pose/control/actuator setters."""
        a = self.atmosphere(backend.f["position/h-sl-ft"], ms)
        inputs = {"atmosphere/P-sl-psf": a["qnh_hpa"]*100/47.88025898,
                  "atmosphere/delta-T": (self.config["atmosphere"]["temperature_sl_c"]-15)*1.8,
                  **{f"atmosphere/wind-{axis}-fps": a[f"wind_{axis}_kt"]*1.687809857
                     for axis in ("north", "east", "down")}}
        if not set(inputs) <= backend.catalog:
            raise ValueError("FDM atmosphere property missing; no silent partial weather")
        for name, value in inputs.items():
            backend.f[name] = value

    def radio(self, station, aircraft, altitude_ft, heading_true, ms):
        d, radial_true = geometry(station, aircraft)
        _, bearing_to = geometry(aircraft, station)
        dh = altitude_ft-station["elevation_ft"]
        # Simplified geometric radio horizon, no refraction/DEM or certified service volume.
        horizon = 1.06*(math.sqrt(max(0, altitude_ft))+math.sqrt(max(0, station["elevation_ft"])))
        reason = "ANALYTIC_REFERENCE"
        if self.fault("radio_loss", ms, station["id"]):
            reason = "STATION_FAILED"
        elif self.fault("radio_ident_mismatch", ms, station["id"]):
            reason = "IDENT_MISMATCH"
        elif d > station["range_nm"] or (station["kind"] != "NDB" and d > horizon):
            reason = "OUT_OF_COVERAGE"
        good = reason == "ANALYTIC_REFERENCE"
        result = {"id": station["id"], "kind": station["kind"], "frequency": station["frequency"],
                  "frequency_unit": "kHz" if station["kind"] == "NDB" else "MHz",
                  "identifier": datum(station["id"] if good else None, "ident", reason),
                  "coverage_model": "SIMPLIFIED_NO_TERRAIN_NO_RF", "simulated": True}
        if station["kind"] == "VOR":
            # DME is a separate range measurement, not invalidated by the VOR cone.
            result["dme_nm"] = datum(math.hypot(d, dh/FT_PER_NM) if good else None, "NM", reason)
            cone = d < .05 or math.degrees(math.atan2(max(0, dh), d*FT_PER_NM)) > 60
            radial = (radial_true-station["variation_east_deg"]) % 360
            delta = wrap(radial-self.config["receiver"]["obs_deg"])
            ambiguous = abs(abs(delta)-90) < .5
            course_error = math.degrees(math.asin(math.sin(math.radians(delta))))
            valid = good and not cone and not ambiguous
            why = reason if not good else "CONE_OR_TO_FROM_AMBIGUITY" if not valid else reason
            result.update(radial_mag_deg=datum(radial if valid else None, "degMAG", why),
                          to_from=datum(("FROM" if abs(delta) < 90 else "TO") if valid else None, "enum", why),
                          cdi=datum(max(-1, min(1, -course_error/10)) if valid else None, "full_scale", why))
        elif station["kind"] == "NDB":
            valid = good and d >= .05
            result.update(bearing_true_deg=datum(bearing_to if valid else None, "degTRUE", reason),
                          relative_bearing_deg=datum((bearing_to-heading_true) % 360 if valid else None, "deg", reason))
        else:
            error = wrap(radial_true-(station["course_true_deg"]+180))
            loc_valid = good and .1 <= d <= station["range_nm"] and abs(error) <= 35
            elevation = math.degrees(math.atan2(dh, d*FT_PER_NM))
            gs_valid = loc_valid and d <= 10 and abs(error) <= 8 and .45*station["gs_deg"] <= elevation <= 1.75*station["gs_deg"]
            result.update(localizer=datum(max(-1, min(1, error/2.5)) if loc_valid else None,
                                          "full_scale", reason if loc_valid else "LOC_OUTSIDE_TRAINING_ENVELOPE"),
                          glideslope=datum(max(-1, min(1, (station["gs_deg"]-elevation)/.7)) if gs_valid else None,
                                          "full_scale", reason if gs_valid else "GS_OUTSIDE_TRAINING_ENVELOPE"))
        return result

    def navigation(self, aircraft, altitude_ft, heading_true, gs_kt, track_true, ms):
        point(aircraft)
        for value in (altitude_ft, heading_true, gs_kt, track_true):
            number(value)
        receiver = self.config["receiver"]
        loss = self.fault("gnss_loss", ms)
        measured = {"lat": aircraft["lat"]+receiver["gnss_error_north_m"]/1852/60,
                    "lon": aircraft["lon"]+receiver["gnss_error_east_m"]/1852/60/math.cos(math.radians(aircraft["lat"]))}
        point(measured)
        route = self.config["route"]
        plan = {"state": "UNAVAILABLE", "reason": "GNSS_LOSS", "simulated": True}
        if not loss:
            direct = receiver["direct_to"]
            if direct is not None:
                if self.direct_origin is None:
                    self.direct_origin = dict(measured)
                start = self.direct_origin
                target = next(p for p in route if p["id"] == direct)
                remaining = 0
                index = None
            else:
                while self.active_leg < len(route)-2:
                    length, _, cross, along = leg_geometry(route[self.active_leg], route[self.active_leg+1], measured)
                    if geometry(measured, route[self.active_leg+1])[0] > .3 and not (along >= length and abs(cross) < 1):
                        break
                    self.active_leg += 1
                index = self.active_leg
                start, target = route[index:index+2]
                remaining = sum(geometry(a, b)[0] for a, b in zip(route[index+1:], route[index+2:]))
            length, course, cross, along = leg_geometry(start, target, measured)
            distance, bearing = geometry(measured, target)
            total = distance+remaining
            closure = gs_kt*math.cos(math.radians(wrap(track_true-bearing)))
            ete = distance/closure*3600 if closure > 1 else None
            plan = {"state": "SIMULATED", "simulated": True, "active_leg": index, "direct_to": direct,
                    "waypoint": target["id"], "dtk_true_deg": course, "bearing_true_deg": bearing,
                    "xtk_right_nm": cross, "along_nm": along, "leg_length_nm": length,
                    "distance_nm": distance, "remaining_route_nm": total,
                    "ete_to_waypoint_s": ete,
                    "eta_to_waypoint_utc": (self.epoch+timedelta(milliseconds=ms, seconds=ete)).isoformat() if ete is not None else None,
                    "arrival": distance <= .3, "eta_model": "CURRENT_CLOSURE_TO_WAYPOINT_NOT_FULL_ROUTE"}
        nearest = sorted(({"id": p["id"], "distance_nm": geometry(measured, p)[0]} for p in route),
                         key=lambda p: (p["distance_nm"], p["id"]))[:5] if not loss else []
        return {"gnss": {"lat": datum(measured["lat"] if not loss else None, "deg"),
                          "lon": datum(measured["lon"] if not loss else None, "deg"),
                          "gs_kt": datum(gs_kt if not loss else None, "kt"),
                          "track_true_deg": datum(track_true if not loss else None, "degTRUE"),
                          "integrity": "NOT_CERTIFIED_NOT_RAIM"},
                "plan": plan, "nearest_route_points": nearest,
                "selected_id": receiver["selected_id"],
                "radios": [self.radio(s, aircraft, altitude_ft, heading_true, ms) for s in self.config["stations"]]}

    def radar(self, ms, size=32):
        if type(size) is not int or not 8 <= size <= 128:
            raise ValueError("radar size")
        number(ms, 0, 3600000)
        r = self.config["radar"]
        lat = r["cell_lat"]+r["velocity_north_kt"]*ms/3600000/60
        lon = r["cell_lon"]+r["velocity_east_kt"]*ms/3600000/60/math.cos(math.radians(r["cell_lat"]))
        west, south, east, north = r["bbox"]
        data = []
        for y in range(size):
            row = []
            for x in range(size):
                p = {"lat": north-(y+.5)*(north-south)/size, "lon": west+(x+.5)*(east-west)/size}
                d, _ = geometry(p, {"lat": lat, "lon": lon})
                row.append(round(r["peak_dbz"]*math.exp(-2*(d/r["radius_nm"])**2), 2))
            data.append(row)
        return {"simulated": True, "product": "SIM_REFLECTIVITY_NOT_ONBOARD_RADAR", "bbox": r["bbox"],
                "crs": "EPSG:4326", "row_order": "NORTH_TO_SOUTH", "unit": "dBZ", "values": data,
                "cell_lat": lat, "cell_lon": lon, "width": size, "height": size}

    def weather_products(self, ms):
        # Publication at ten-minute boundaries; delay and offline freeze retrieval time.
        number(ms, 0, 3600000)
        issue = (ms//600000)*600000
        if self.fault("wx_delay", ms):
            issue -= 1800000
        if self.fault("wx_offline", ms):
            starts = [f["start_ms"] for f in self.config["faults"]
                      if f["kind"] == "wx_offline" and f["start_ms"] <= ms < f["end_ms"]]
            issue = (min(starts)//600000)*600000
        if self.fault("wx_clock_skew", ms):
            issue = ms+600000
        age = ms-issue
        state = "CLOCK_SKEW" if age < -5000 else "CURRENT" if age <= 600000 else "STALE" if age <= 1200000 else "EXPIRED"
        observed = self.epoch+timedelta(milliseconds=issue)
        # Clock-skew fault corrupts the product timestamp, not the physical clock.
        a = self.atmosphere(0, min(ms, max(0, issue)))
        speed = math.hypot(a["wind_north_kt"], a["wind_east_kt"])
        wind_from = math.degrees(math.atan2(-a["wind_east_kt"], -a["wind_north_kt"])) % 360
        direction = (int((wind_from+5)//10)*10) % 360 or 360
        wind = "00000KT" if speed < .5 else f"{direction:03d}{round(speed):02d}KT"
        def signed_temp(v):
            return ("M" if v < 0 else "")+f"{abs(round(v)):02d}"
        temperature = signed_temp(a["temperature_c"])
        # Dewpoint is not a humidity model; omitted rather than fabricated.
        cloud = min(999, int(a["cloud_base_ft"]//100))
        rain = " +RA" if a["rain_rate_mmh"] >= 10 else " RA" if a["rain_rate_mmh"] > 0 else ""
        raw = (f"METAR ZZZZ {observed:%d%H%M}Z {wind} {int(a['visibility_m']):04d}"
               f"{rain} BKN{cloud:03d} {temperature}// Q{round(a['qnh_hpa']):04d} RMK SIM TRAINING")
        valid_from = observed.replace(minute=0, second=0, microsecond=0)
        end = valid_from+timedelta(hours=6)
        taf = (f"TAF ZZZZ {observed:%d%H%M}Z {valid_from:%d%H}/{end:%d%H} {wind} "
               f"{int(a['visibility_m']):04d}{rain} BKN{cloud:03d} RMK SIM TRAINING")
        return {"simulated": True, "safety_use_allowed": False, "icao": "ZZZZ", "state": state,
                "transport": "OFFLINE" if self.fault("wx_offline", ms) else "SIMULATED",
                "age_ms": age, "observation_utc": observed.isoformat(),
                "forecast_valid_from_utc": valid_from.isoformat(), "valid_to_utc": end.isoformat(),
                "sim_time_ms": ms, "issue_sim_time_ms": issue,
                "raw_metar": raw, "raw_taf": taf,
                "forecast_model": "PERSISTENCE_TRAINING_NOT_PREDICTION", "observation": a,
                "display_allowed": state in ("CURRENT", "STALE"),
                "radar": {"issue_sim_time_ms": issue, "bbox": self.config["radar"]["bbox"],
                          "cache_namespace": "SIM_ONLY", "generator": "ANALYTIC_GAUSSIAN_CELL"}}

    def frame(self, flight, ms, fdm_atmosphere=None):
        if type(ms) is not int or not 0 <= ms <= 3600000 or ms < self.last_ms:
            raise ValueError("world requires ordered simulation clock; new instance for seek/reset")
        self.last_ms = ms
        get = lambda key: number(flight[key]["value"])
        aircraft = {"lat": get("latitude_deg"), "lon": get("longitude_deg")}
        altitude, heading = get("altitude_msl_ft"), get("heading_true_deg")
        n, e = get("north_fps"), get("east_fps")
        track = math.degrees(math.atan2(e, n)) % 360
        a = self.atmosphere(altitude, ms)
        if fdm_atmosphere is not None:
            exact(fdm_atmosphere, "pressure_hpa temperature_c density_kg_m3")
            number(fdm_atmosphere["pressure_hpa"], 1, 1200)
            number(fdm_atmosphere["temperature_c"], -150, 100)
            number(fdm_atmosphere["density_kg_m3"], .001, 3)
            a.update(fdm_atmosphere)
        r = self.config["receiver"]
        inverse = lambda reference: 288.15/.0065*(1-(a["pressure_hpa"]/reference)**(287.05287*.0065/9.80665))/.3048
        cas = flight["cas_kt"]["value"]
        return {"schema": SCHEMA, "simulated": True, "flight_approval": False, "sim_time_ms": ms,
                "dataset_id": self.config["dataset_id"], "truth_weather": a,
                "fdm_atmosphere": fdm_atmosphere,
                "sensors": {"ias_kt": datum(max(0, number(cas)+r["ias_bias_kt"]) if cas is not None else None,
                                            "kt", "CAS_PLUS_EXPLICIT_INSTALLATION_BIAS_REFERENCE"),
                            "pressure_altitude_ft": datum(inverse(1013.25), "ft"),
                            "indicated_altitude_ft": datum(inverse(r["altimeter_hpa"]), "ft"),
                            "heading_mag_deg": datum((heading-r["variation_east_deg"]) % 360, "degMAG")},
                "navigation": self.navigation(aircraft, altitude, heading, get("gs_kt"), track, ms),
                "weather_products": self.weather_products(ms)}


def radar_png(radar):
    """Small RGBA PNG fixture, georeferencing stays in associated metadata."""
    def chunk(kind, data):
        return struct.pack(">I", len(data))+kind+data+struct.pack(">I", zlib.crc32(kind+data)&0xffffffff)
    rows = []
    for row in radar["values"]:
        pixels = bytearray()
        for value in row:
            color = (0, 0, 0, 0) if value < 5 else (0, 190, 40, 180) if value < 30 else (250, 200, 0, 210) if value < 45 else (230, 20, 30, 230)
            pixels.extend(color)
        rows.append(b"\0"+bytes(pixels))
    return (b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR", struct.pack(">IIBBBBB", radar["width"], radar["height"], 8, 6, 0, 0, 0))
            +chunk(b"IDAT", zlib.compress(b"".join(rows)))+chunk(b"IEND", b""))


def config_digest(config):
    return hashlib.sha256(json.dumps(validate(config), sort_keys=True, separators=(",", ":"), allow_nan=False).encode()).hexdigest()
