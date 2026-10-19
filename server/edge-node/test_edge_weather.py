import json
import io
import contextlib
import subprocess
import sys
import threading
import socket
import tempfile
import unittest
from unittest.mock import patch
from datetime import datetime, timezone
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.error import HTTPError, URLError

import edge_weather


NOW = 1_800_000_000.0


def record(age_seconds=0, *, metar="TEST METAR", taf=None, fetched_utc=None):
    timestamp = NOW - age_seconds
    return {
        "schema": edge_weather.CACHE_SCHEMA,
        "provider_payload": {"metar": metar, "taf": taf},
        "provider_url": "https://mappe.casa.lan",
        "fetched_utc": fetched_utc
        or datetime.fromtimestamp(timestamp, timezone.utc).isoformat(),
    }


class EdgeWeatherTest(unittest.TestCase):
    def test_blank_bulletins_are_no_report(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory, "LIPH", record(metar=" \n ", taf="\t"))
            self.assertEqual(edge_weather.cached(directory, "LIPH", now=NOW)["state"], "NO_REPORT")

    def test_fractional_age_does_not_extend_freshness_limits(self):
        for age, expected in [(600.01, "STALE"), (1200.01, "EXPIRED")]:
            with self.subTest(age=age), tempfile.TemporaryDirectory() as directory:
                self.write(directory, "LIPH", record(age))
                self.assertEqual(edge_weather.cached(directory, "LIPH", now=NOW)["state"], expected)

    def test_non_finite_local_clock_is_not_current(self):
        for now in (float("nan"), float("inf"), float("-inf")):
            with self.subTest(now=now), tempfile.TemporaryDirectory() as directory:
                self.write(directory, "LIPH", record())
                item = edge_weather.cached(directory, "LIPH", now=now)
                self.assertEqual(item["state"], "CLOCK_SKEW")
                self.assertFalse(item["display_allowed"])

    def test_strict_json_rejects_duplicate_fields_non_finite_and_bad_utf8(self):
        for raw in (b'{"metar":"one","metar":"two"}', b'{"n":NaN}',
                    b'{"n":Infinity}', b'{"n":1e999}', b'\xff', b'{'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                edge_weather.limited_json(io.BytesIO(raw), 256)

    def test_stream_read_is_bounded_by_limit_plus_one(self):
        stream = io.BytesIO(b"x" * 100)
        with self.assertRaises(ValueError): edge_weather.limited_json(stream, 10)
        self.assertEqual(stream.tell(), 11)

    def test_station_mismatch_is_invalid_in_envelope_and_payload(self):
        for location in ("envelope", "provider"):
            with self.subTest(location=location), tempfile.TemporaryDirectory() as directory:
                payload = record()
                (payload if location == "envelope" else payload["provider_payload"])["icao"] = "LIPZ"
                self.write(directory, "LIPH", payload)
                self.assertEqual(edge_weather.cached(directory, "LIPH", now=NOW)["state"], "INVALID")

    def test_missing_fields_and_oversize_bulletin_rejected(self):
        for payload in ({"error": "unavailable"}, {"metar": "x" * (edge_weather.MAX_BULLETIN_CHARACTERS + 1)},
                        {"taf": True}, {"metar": "report", "station": "LIPZ"}):
            with self.subTest(payload_type=type(payload)), self.assertRaises(ValueError):
                edge_weather.validate_provider_payload(payload, "LIPH")

    def test_cached_oversize_json_is_invalid(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "LIPH.json").write_bytes(b" " * (edge_weather.MAX_CACHE_BYTES + 1))
            self.assertEqual(edge_weather.cached(directory, "LIPH", now=NOW)["state"], "INVALID")

    def test_network_http_timeout_and_truncated_payload_preserve_last_good(self):
        for error in (URLError("offline"), HTTPError("https://example.invalid", 503, "unavailable", {}, None),
                      TimeoutError("timeout")):
            with self.subTest(error=type(error)), tempfile.TemporaryDirectory() as directory:
                self.write(directory, "LIPH", record())
                before = Path(directory, "LIPH.json").read_bytes()
                with patch.object(edge_weather.urllib.request, "urlopen", side_effect=error):
                    with self.assertRaises((URLError, TimeoutError)):
                        edge_weather.sync_one(directory, "https://example.invalid", "LIPH")
                self.assertEqual(Path(directory, "LIPH.json").read_bytes(), before)
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory, "LIPH", record())
            before = Path(directory, "LIPH.json").read_bytes()
            for raw in (b'{"metar":', b" " * (edge_weather.MAX_RESPONSE_BYTES + 1)):
                with patch.object(edge_weather.urllib.request, "urlopen", return_value=io.BytesIO(raw)):
                    with self.assertRaises(ValueError): edge_weather.sync_one(directory, "https://example.invalid", "LIPH")
                self.assertEqual(Path(directory, "LIPH.json").read_bytes(), before)

    def test_atomic_write_fsync_or_replace_failure_preserves_cache_and_cleans_temp(self):
        for operation in ("fsync", "replace"):
            with self.subTest(operation=operation), tempfile.TemporaryDirectory() as directory:
                target = Path(directory, "LIPH.json")
                self.write(directory, "LIPH", record())
                before = target.read_bytes()
                with patch.object(edge_weather.os, operation, side_effect=OSError("injected I/O fault")):
                    with self.assertRaises(OSError): edge_weather.atomic_json(target, record(100))
                self.assertEqual(target.read_bytes(), before)
                self.assertEqual(list(Path(directory).iterdir()), [target])

    def test_serialization_failure_never_replaces_valid_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory, "LIPH.json"); self.write(directory, "LIPH", record())
            before = target.read_bytes()
            for bad in ({"number": float("nan")}, {"huge": "x" * edge_weather.MAX_CACHE_BYTES}):
                with self.assertRaises(ValueError): edge_weather.atomic_json(target, bad)
                self.assertEqual(target.read_bytes(), before)
                self.assertEqual(list(Path(directory).iterdir()), [target])

    def test_restart_reopens_persisted_cache_in_separate_interpreter(self):
        with tempfile.TemporaryDirectory() as directory:
            edge_weather.atomic_json(Path(directory, "LIPH.json"), record(800))
            code = ("import sys,json;sys.path.insert(0,sys.argv[1]);import edge_weather;"
                    "print(json.dumps(edge_weather.cached(sys.argv[2],'LIPH',now=" + str(NOW) + "))) ")
            result = subprocess.run([sys.executable, "-I", "-c", code, str(Path(edge_weather.__file__).parent), directory],
                                    capture_output=True, text=True, timeout=15, check=True)
            item = json.loads(result.stdout)
            self.assertEqual(item["state"], "STALE")
            self.assertEqual(item["age_seconds"], 800)
            self.assertFalse(item["safety_use_allowed"])

    def test_freshness_claim_is_only_fetch_time_not_bulletin_time(self):
        with tempfile.TemporaryDirectory() as directory:
            payload = record()
            payload.update(freshness_basis="OBSERVATION_TIME", provider_report_time_validated=True)
            self.write(directory, "LIPH", payload)
            item = edge_weather.cached(directory, "LIPH", now=NOW)
            self.assertEqual(item["freshness_basis"], "FETCH_TIME_ONLY")
            self.assertFalse(item["provider_report_time_validated"])

    def test_loopback_http_sync_and_offline_reopen(self):
        class FixtureHandler(BaseHTTPRequestHandler):
            def do_GET(self):
                raw = b'{"icao":"LIPH","metar":"TEST ONLY METAR","taf":null}'
                self.send_response(200); self.send_header("Content-Length", str(len(raw))); self.end_headers()
                self.wfile.write(raw)
            def log_message(self, *args): pass
        server = ThreadingHTTPServer(("127.0.0.1", 0), FixtureHandler)
        worker = threading.Thread(target=server.serve_forever, daemon=True); worker.start()
        try:
            with tempfile.TemporaryDirectory() as directory:
                edge_weather.sync_one(directory, "http://127.0.0.1:" + str(server.server_port), "LIPH")
                item = edge_weather.cached(directory, "LIPH")
                self.assertEqual(item["state"], "CURRENT")
                self.assertEqual(item["provider_payload"]["metar"], "TEST ONLY METAR")
                self.assertEqual(item["icao"], "LIPH")
        finally:
            server.shutdown(); server.server_close(); worker.join(timeout=5)

    def test_non_text_bulletin_is_not_available(self):
        for value in (True, 123, {"raw": "report"}, ["report"]):
            with self.subTest(value=value), tempfile.TemporaryDirectory() as directory:
                self.write(directory, "LIPH", record(metar=value))
                item = edge_weather.cached(directory, "LIPH", now=NOW)
                self.assertEqual(item["state"], "INVALID")
                self.assertFalse(item["display_allowed"])

    def test_failed_validation_preserves_last_good_cache_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory, "LIPH", record())
            target = Path(directory, "LIPH.json")
            before = target.read_bytes()
            with patch.object(edge_weather.urllib.request, "urlopen", return_value=io.BytesIO(b'{"metar":true}')):
                with self.assertRaises(ValueError):
                    edge_weather.sync_one(directory, "https://example.invalid", "LIPH")
            self.assertEqual(target.read_bytes(), before)

    def write(self, directory, station, payload):
        Path(directory, station + ".json").write_text(
            json.dumps(payload), encoding="utf-8"
        )

    def test_missing_station_is_explicit_no_data(self):
        with tempfile.TemporaryDirectory() as directory:
            item = edge_weather.cached(directory, "LIPH", now=NOW)
            self.assertEqual(item["state"], "NO_DATA")
            self.assertTrue(item["stale"])
            self.assertFalse(item["display_allowed"])

    def test_corrupt_json_is_invalid(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, 'LIPH.json').write_text('{broken', encoding='utf-8')
            item = edge_weather.cached(directory, 'LIPH', now=NOW)
            self.assertEqual('INVALID', item['state'])
            self.assertFalse(item['display_allowed'])

    def test_cached_flags_cannot_override_computed_freshness(self):
        with tempfile.TemporaryDirectory() as directory:
            payload = record(1201)
            payload.update(state='CURRENT', display_allowed=True, safety_use_allowed=True)
            self.write(directory, 'LIPH', payload)
            item = edge_weather.cached(directory, 'LIPH', now=NOW)
            self.assertEqual('EXPIRED', item['state'])
            self.assertFalse(item['display_allowed'])
            self.assertFalse(item['safety_use_allowed'])

    def test_small_clock_jitter_and_exact_future_limit(self):
        for age, expected in [(-5, 'CURRENT'), (-6, 'CLOCK_SKEW')]:
            with self.subTest(age=age), tempfile.TemporaryDirectory() as directory:
                self.write(directory, 'LIPH', record(age))
                self.assertEqual(expected, edge_weather.cached(directory, 'LIPH', now=NOW)['state'])

    def test_current_stale_and_expired_thresholds_are_deterministic(self):
        cases = ((600, "CURRENT", True), (601, "STALE", True),
                 (1200, "STALE", True), (1201, "EXPIRED", False))
        for age, expected, display_allowed in cases:
            with self.subTest(age=age), tempfile.TemporaryDirectory() as directory:
                self.write(directory, "LIPH", record(age))
                item = edge_weather.cached(directory, "LIPH", now=NOW)
                self.assertEqual(item["state"], expected)
                self.assertEqual(item["age_seconds"], age)
                self.assertEqual(item["display_allowed"], display_allowed)
                self.assertFalse(item["safety_use_allowed"])

    def test_future_timestamp_fails_closed_as_clock_skew(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory, "LIPH", record(-30))
            item = edge_weather.cached(directory, "LIPH", now=NOW)
            self.assertEqual(item["state"], "CLOCK_SKEW")
            self.assertEqual(item["reason"], "FETCH_TIMESTAMP_IN_FUTURE")
            self.assertIsNone(item["age_seconds"])
            self.assertFalse(item["display_allowed"])

    def test_invalid_schema_timestamp_and_payload_fail_closed(self):
        cases = (
            {"schema": "wrong", "provider_payload": {}, "fetched_utc": "x"},
            {"schema": edge_weather.CACHE_SCHEMA, "provider_payload": [], "fetched_utc": "x"},
            {"schema": edge_weather.CACHE_SCHEMA, "provider_payload": {}, "fetched_utc": "x"},
        )
        for payload in cases:
            with self.subTest(payload=payload), tempfile.TemporaryDirectory() as directory:
                self.write(directory, "LIPH", payload)
                item = edge_weather.cached(directory, "LIPH", now=NOW)
                self.assertEqual(item["state"], "INVALID")
                self.assertFalse(item["display_allowed"])

    def test_station_without_bulletin_is_not_reported_available(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory, "LIDB", record(10, metar=None, taf=None))
            item = edge_weather.cached(directory, "LIDB", now=NOW)
            self.assertEqual(item["state"], "NO_REPORT")
            self.assertFalse(item["has_report"])

    def test_invalid_station_cannot_escape_cache_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            for station in ("../x", "LIPHH", "LI P", ""):
                with self.subTest(station=station), self.assertRaises(ValueError):
                    edge_weather.cached(directory, station, now=NOW)

    def test_status_reports_each_station_state(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(directory, "LIPH", record(0))
            self.write(directory, "LIDB", record(800))
            self.write(directory, "LIPZ", record(1300))
            Path(directory, "not-a-station.json").write_text("{}", encoding="utf-8")
            status = edge_weather.cache_status(directory, now=NOW)
            self.assertEqual(status["schema"], "efis-next.edge-weather-status.v2")
            self.assertEqual(status["station_count"], 3)
            self.assertEqual(status["states"], {"CURRENT": 1, "STALE": 1, "EXPIRED": 1})
            self.assertFalse(status["safety_use_allowed"])

    def test_pinned_resolver_preserves_named_host_policy(self):
        mapping = edge_weather.parse_resolve("mappe.casa.lan=192.168.1.6")
        with edge_weather.temporary_resolver(mapping):
            result = socket.getaddrinfo("mappe.casa.lan", 443, type=socket.SOCK_STREAM)
        self.assertTrue(result)
        self.assertEqual(result[0][4][0], "192.168.1.6")

    def test_invalid_resolver_is_rejected(self):
        with self.assertRaises(ValueError):
            edge_weather.parse_resolve("mappe.casa.lan=not-an-ip")


if __name__ == "__main__":
    unittest.main()
