#!/usr/bin/env python3
"""Fail-safe local aviation-weather cache for EFIS EDGE V2."""

import argparse
import base64
import contextlib
import json
import os
import re
import socket
import ssl
import tempfile
import time
import urllib.request
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


CURRENT_MAX_AGE_SECONDS = 10 * 60
STALE_MAX_AGE_SECONDS = 20 * 60
FUTURE_TOLERANCE_SECONDS = 5
ICAO_RE = re.compile(r"^[A-Z0-9]{4}$")
CACHE_SCHEMA = "efis-next.edge-weather-cache.v1"


def normalized_icao(value):
    station = str(value).strip().upper()
    if ICAO_RE.fullmatch(station) is None:
        raise ValueError("ICAO must contain exactly four ASCII letters or digits")
    return station


def parse_resolve(value):
    if not value:
        return None
    try:
        hostname, address = value.rsplit("=", 1)
        socket.inet_pton(socket.AF_INET6 if ":" in address else socket.AF_INET, address)
    except (ValueError, OSError) as exc:
        raise ValueError("--resolve must be HOSTNAME=IP") from exc
    if not hostname or any(character.isspace() for character in hostname):
        raise ValueError("--resolve must be HOSTNAME=IP")
    return hostname.lower(), address


@contextlib.contextmanager
def temporary_resolver(resolve):
    """Pin one hostname to an IP while preserving HTTPS SNI and Host."""
    if not resolve:
        yield
        return
    expected_host, address = resolve
    original = socket.getaddrinfo

    def mapped(host, port, *args, **kwargs):
        target = address if str(host).lower() == expected_host else host
        return original(target, port, *args, **kwargs)

    socket.getaddrinfo = mapped
    try:
        yield
    finally:
        socket.getaddrinfo = original


def atomic_json(path, payload):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=path.parent)
    temporary_path = Path(temporary)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_path, path)
    finally:
        temporary_path.unlink(missing_ok=True)


def _result(station, state, *, age_seconds=None, has_report=False, reason=None):
    payload = {
        "schema": CACHE_SCHEMA,
        "icao": station,
        "state": state,
        "stale": state != "CURRENT",
        "age_seconds": age_seconds,
        "has_report": has_report,
        "display_allowed": state in {"CURRENT", "STALE"} and has_report,
        "safety_use_allowed": False,
        "freshness_thresholds_seconds": {
            "current_max": CURRENT_MAX_AGE_SECONDS,
            "stale_max": STALE_MAX_AGE_SECONDS,
        },
    }
    if reason:
        payload["reason"] = reason
    return payload


def _fetched_timestamp(data):
    value = data.get("fetched_utc")
    if not isinstance(value, str):
        raise ValueError("fetched_utc missing")
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise ValueError("fetched_utc must include a timezone")
    return parsed.astimezone(timezone.utc)


def cached(cache, icao, now=None):
    station = normalized_icao(icao)
    path = Path(cache) / (station + ".json")
    if not path.is_file() or path.is_symlink():
        return _result(station, "NO_DATA", reason="CACHE_FILE_MISSING")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(data, dict) or data.get("schema") != CACHE_SCHEMA:
            raise ValueError("cache schema mismatch")
        report = data.get("provider_payload")
        if not isinstance(report, dict):
            raise ValueError("provider_payload must be an object")
        fetched = _fetched_timestamp(data)
    except (OSError, ValueError, TypeError) as exc:
        return _result(station, "INVALID", reason=str(exc))

    current_time = time.time() if now is None else float(now)
    age_float = current_time - fetched.timestamp()
    has_report = bool(report.get("metar") or report.get("taf"))
    if age_float < -FUTURE_TOLERANCE_SECONDS:
        computed = _result(
            station,
            "CLOCK_SKEW",
            age_seconds=None,
            has_report=has_report,
            reason="FETCH_TIMESTAMP_IN_FUTURE",
        )
    else:
        age = max(0, int(age_float))
        if not has_report:
            state = "NO_REPORT"
        elif age <= CURRENT_MAX_AGE_SECONDS:
            state = "CURRENT"
        elif age <= STALE_MAX_AGE_SECONDS:
            state = "STALE"
        else:
            state = "EXPIRED"
        computed = _result(station, state, age_seconds=age, has_report=has_report)

    result = dict(data)
    # Cache-controlled fields always override untrusted provider content.
    result.update(computed)
    result.update({
        "edge_source": "CENTRAL_CACHE",
        "edge_cached_utc": fetched.isoformat(),
    })
    return result


def cache_status(cache, now=None):
    directory = Path(cache)
    states = {}
    stations = []
    if directory.is_dir():
        for path in sorted(directory.glob("*.json")):
            try:
                station = normalized_icao(path.stem)
            except ValueError:
                continue
            item = cached(directory, station, now=now)
            states[item["state"]] = states.get(item["state"], 0) + 1
            stations.append({
                "icao": station,
                "state": item["state"],
                "age_seconds": item["age_seconds"],
            })
    return {
        "schema": "efis-next.edge-weather-status.v2",
        "mode": "SHADOW_CACHE",
        "station_count": len(stations),
        "states": states,
        "stations": stations,
        "safety_use_allowed": False,
    }


def sync_one(cache, base_url, icao, username=None, password=None, ca_file=None, resolve=None):
    station = normalized_icao(icao)
    headers = {"User-Agent": "EFIS-EDGE-V2/0.2"}
    if username or password:
        if not username or not password:
            raise ValueError("both EDGE weather credentials are required")
        token = base64.b64encode(f"{username}:{password}".encode()).decode()
        headers["Authorization"] = "Basic " + token
    request = urllib.request.Request(
        base_url.rstrip("/") + "/api/metar/" + station,
        headers=headers,
    )
    context = ssl.create_default_context(cafile=ca_file) if ca_file else None
    with temporary_resolver(resolve):
        with urllib.request.urlopen(request, timeout=20, context=context) as response:
            payload = json.load(response)
    if not isinstance(payload, dict):
        raise ValueError("weather provider response must be a JSON object")
    cached_payload = {
        "schema": CACHE_SCHEMA,
        "provider_payload": payload,
        "provider_url": base_url,
        "fetched_utc": datetime.now(timezone.utc).isoformat(),
    }
    atomic_json(Path(cache) / (station + ".json"), cached_payload)
    return cached_payload


class Handler(BaseHTTPRequestHandler):
    cache = None

    def do_GET(self):
        if self.path == "/v1/weather/status":
            payload = cache_status(self.cache)
        elif self.path.startswith("/v1/weather/") and len(self.path.split("/")) == 4:
            try:
                payload = cached(self.cache, self.path.rsplit("/", 1)[-1])
            except ValueError as exc:
                self.send_error(400, str(exc))
                return
        else:
            self.send_error(404)
            return
        body = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        return


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cache", default="cache/weather")
    parser.add_argument("--sync-url")
    parser.add_argument("--icao", action="append", default=[])
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=18092)
    parser.add_argument("--serve", action="store_true")
    parser.add_argument("--ca-file")
    parser.add_argument("--resolve", help="pin HTTPS hostname without changing system DNS: HOSTNAME=IP")
    args = parser.parse_args()
    resolve = parse_resolve(args.resolve)
    stations = [normalized_icao(value) for value in (args.icao or ["LIPH", "LIDB"])]
    if args.sync_url:
        for station in stations:
            sync_one(
                args.cache,
                args.sync_url,
                station,
                os.getenv("EFIS_EDGE_USER"),
                os.getenv("EFIS_EDGE_PASSWORD"),
                args.ca_file,
                resolve,
            )
    if args.serve:
        Handler.cache = args.cache
        ThreadingHTTPServer((args.bind, args.port), Handler).serve_forever()
    else:
        print(json.dumps({station: cached(args.cache, station) for station in stations}))


if __name__ == "__main__":
    main()
