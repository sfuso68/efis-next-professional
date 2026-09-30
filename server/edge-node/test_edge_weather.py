import json
import socket
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path

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
