"""Analytic offline oracles for the SIM world, never operational nav acceptance."""
import copy
import json
import math
from pathlib import Path
import tempfile
import unittest

from tools.simulation import sil_world as w
from tools.simulation import jsbsim_session as s
from tests.test_jsbsim_session import backend, fixture

CONFIG = Path(__file__).resolve().parents[1]/"config/simulation/treviso-belluno-world-reference.json"


def config():
    return json.loads(CONFIG.read_text(encoding="utf-8"))


def flight(lat=45.65, lon=12.2, altitude=5000, heading=0, gs=90):
    f = backend().frame("cruise", "world-test")["flight"]
    for key, value in {"latitude_deg": lat, "longitude_deg": lon, "altitude_msl_ft": altitude,
                       "heading_true_deg": heading, "gs_kt": gs, "north_fps": gs*1.687809857,
                       "east_fps": 0, "cas_kt": 90}.items():
        f[key]["value"] = value
    return f


def station(kind="VOR"):
    return dict(id="SIMTEST", kind=kind, lat=0, lon=0, elevation_ft=0,
                frequency=113, variation_east_deg=0, range_nm=100, course_true_deg=0, gs_deg=3)


class WorldTests(unittest.TestCase):
    def test_configuration_deep_copy_and_digest(self):
        c = config()
        world = w.World(c)
        before = w.config_digest(c)
        c["atmosphere"]["qnh_hpa"] = 1013
        self.assertEqual(world.config["atmosphere"]["qnh_hpa"], 1005)
        self.assertNotEqual(before, w.config_digest(c))

    def test_schema_rejects_unknown_and_nonfinite(self):
        for mutate in (lambda c: c.update(unexpected=True),
                       lambda c: c.update(schema="LIVE"),
                       lambda c: c["atmosphere"].update(qnh_hpa=math.nan),
                       lambda c: c["receiver"].update(obs_deg=True),
                       lambda c: c.update(epoch_utc="2026-01-01T00:00:00"),
                       lambda c: c.update(seed=-1)):
            c = config(); mutate(c)
            with self.assertRaises(ValueError):
                w.World(c)

    def test_invalid_geometry_dataset_and_faults(self):
        for mutate in (lambda c: c["stations"][0].update(id="REALVOR"),
                       lambda c: c["stations"][0].update(kind="UNKNOWN"),
                       lambda c: c["route"][1].update(lat=c["route"][0]["lat"], lon=c["route"][0]["lon"]),
                       lambda c: c["faults"][0].update(end_ms=0),
                       lambda c: c["faults"][0].update(kind="ACTUATOR"),
                       lambda c: c["receiver"].update(selected_id="NOPE"),
                       lambda c: c["radar"].update(bbox=[2, 1, 0, 3]),
                       lambda c: c["atmosphere"]["wind_layers"][1].update(altitude_ft=0)):
            c = config(); mutate(c)
            with self.assertRaises(ValueError):
                w.World(c)

    def test_geodesy_cardinal_and_antimeridian(self):
        origin = {"lat": 0, "lon": 0}
        d, b = w.geometry(origin, {"lat": 1, "lon": 0})
        self.assertAlmostEqual(d, 60.04046, places=4)
        self.assertAlmostEqual(b, 0)
        self.assertAlmostEqual(w.geometry(origin, {"lat": 0, "lon": 1})[1], 90)
        self.assertLess(w.geometry({"lat": 0, "lon": 179.9}, {"lat": 0, "lon": -179.9})[0], 13)

    def test_cross_track_right_positive(self):
        length, course, cross, along = w.leg_geometry({"lat": 0, "lon": 0}, {"lat": 1, "lon": 0}, {"lat": .5, "lon": .1})
        self.assertAlmostEqual(course, 0)
        self.assertAlmostEqual(cross, 6.0038, places=3)
        self.assertTrue(0 < along < length)

    def test_atmosphere_isa_reference(self):
        c = config(); c["atmosphere"].update(qnh_hpa=1013.25, temperature_sl_c=15)
        world = w.World(c)
        sea = world.atmosphere(0, 0)
        self.assertAlmostEqual(sea["pressure_hpa"], 1013.25)
        self.assertAlmostEqual(sea["density_kg_m3"], 1.225, places=4)
        high = world.atmosphere(10000, 0)
        self.assertAlmostEqual(high["pressure_hpa"], 696.817, places=2)
        self.assertAlmostEqual(high["temperature_c"], -4.812, places=3)

    def test_wind_shear_gust_deterministic(self):
        world = w.World(config())
        low = world.atmosphere(0, 0)
        mid = world.atmosphere(2500, 0)
        high = world.atmosphere(5000, 0)
        self.assertAlmostEqual(mid["wind_north_kt"], (low["wind_north_kt"]+high["wind_north_kt"])/2)
        self.assertNotEqual(low["wind_east_kt"], world.atmosphere(0, 3000)["wind_east_kt"])
        self.assertEqual(low, w.World(config()).atmosphere(0, 0))

    def test_fdm_writes_only_atmosphere_and_missing_fails(self):
        b = backend(); world = w.World(config())
        names = {"atmosphere/P-sl-psf", "atmosphere/delta-T", "atmosphere/wind-north-fps",
                 "atmosphere/wind-east-fps", "atmosphere/wind-down-fps"}
        before = dict(b.f)
        with self.assertRaises(ValueError):
            world.apply_fdm(b, 0)
        self.assertEqual(before, b.f)
        b.catalog |= names
        world.apply_fdm(b, 0)
        self.assertEqual(set(b.f)-set(before), names)
        self.assertTrue(all(b.f[k] == v for k, v in before.items()))

    def test_altimeter_magnetic_ias_semantics(self):
        world = w.World(config())
        output = world.frame(flight(heading=2), 0)
        sensors = output["sensors"]
        self.assertEqual(sensors["heading_mag_deg"]["value"], 359)
        self.assertEqual(sensors["ias_kt"]["value"], 89)
        self.assertNotEqual(sensors["pressure_altitude_ft"], sensors["indicated_altitude_ft"])
        self.assertNotAlmostEqual(sensors["indicated_altitude_ft"]["value"], 5000, places=0)

    def test_actual_fdm_pressure_drives_sensor(self):
        actual = {"pressure_hpa": 1013.25, "temperature_c": 15, "density_kg_m3": 1.225}
        sensors = w.World(config()).frame(flight(), 0, actual)["sensors"]
        self.assertAlmostEqual(sensors["pressure_altitude_ft"]["value"], 0)
        with self.assertRaises(ValueError):
            w.World(config()).frame(flight(), 0, {**actual, "pressure_hpa": 0})

    def test_vor_radial_to_from_and_cdi_direction(self):
        c = config(); c["receiver"]["obs_deg"] = 0
        world = w.World(c); vor = station()
        outbound = world.radio(vor, {"lat": .1, "lon": 0}, 5000, 0, 0)
        inbound = world.radio(vor, {"lat": -.1, "lon": 0}, 5000, 0, 0)
        self.assertEqual(outbound["to_from"]["value"], "FROM")
        self.assertEqual(inbound["to_from"]["value"], "TO")
        self.assertAlmostEqual(inbound["cdi"]["value"], 0)
        for lat in (-.1, .1):
            self.assertLess(world.radio(vor, {"lat": lat, "lon": .01}, 5000, 0, 0)["cdi"]["value"], 0)

    def test_vor_station_variation_not_aircraft(self):
        c = config(); c["receiver"].update(obs_deg=357, variation_east_deg=-10)
        vor = station(); vor["variation_east_deg"] = 3
        signal = w.World(c).radio(vor, {"lat": .1, "lon": 0}, 5000, 0, 0)
        self.assertAlmostEqual(signal["radial_mag_deg"]["value"], 357)
        self.assertAlmostEqual(signal["cdi"]["value"], 0)

    def test_dme_slant_works_inside_vor_cone(self):
        signal = w.World(config()).radio(station(), {"lat": 0, "lon": 0}, w.FT_PER_NM, 0, 0)
        self.assertAlmostEqual(signal["dme_nm"]["value"], 1)
        self.assertIsNone(signal["radial_mag_deg"]["value"])

    def test_radio_coverage_and_failure_null_not_centered(self):
        world = w.World(config())
        out = world.radio(station(), {"lat": 3, "lon": 0}, 100, 0, 0)
        self.assertIsNone(out["cdi"]["value"])
        self.assertIsNone(out["dme_nm"]["value"])
        c = config(); c["faults"] = [{"kind": "radio_loss", "start_ms": 0, "end_ms": 1000, "target": "ALL"}]
        self.assertIsNone(w.World(c).radio(station(), {"lat": .1, "lon": 0}, 5000, 0, 0)["identifier"]["value"])

    def test_ndb_relative_bearing(self):
        world = w.World(config())
        signal = world.radio(station("NDB"), {"lat": -.1, "lon": 0}, 5000, 270, 0)
        self.assertAlmostEqual(signal["relative_bearing_deg"]["value"], 90)

    def test_ils_front_back_and_needles(self):
        world = w.World(config()); ils = station("ILS")
        d = w.geometry(ils, {"lat": -.05, "lon": 0})[0]
        altitude = math.tan(math.radians(3))*d*w.FT_PER_NM
        signal = world.radio(ils, {"lat": -.05, "lon": 0}, altitude, 0, 0)
        self.assertAlmostEqual(signal["localizer"]["value"], 0)
        self.assertAlmostEqual(signal["glideslope"]["value"], 0)
        self.assertIsNone(world.radio(ils, {"lat": .05, "lon": 0}, altitude, 180, 0)["localizer"]["value"])
        self.assertLess(world.radio(ils, {"lat": -.05, "lon": .001}, altitude, 0, 0)["localizer"]["value"], 0)
        self.assertLess(world.radio(ils, {"lat": -.05, "lon": 0}, altitude*1.1, 0, 0)["glideslope"]["value"], 0)

    def test_gnss_loss_does_not_remove_radio_and_recovers(self):
        world = w.World(config())
        for ms, available in ((19999, True), (20000, False), (29999, False), (30000, True)):
            nav = world.frame(flight(), ms)["navigation"]
            self.assertEqual(nav["gnss"]["lat"]["value"] is not None, available)
            self.assertEqual(nav["plan"]["state"] == "SIMULATED", available)
            self.assertIsNotNone(nav["radios"][0]["radial_mag_deg"]["value"])

    def test_ident_mismatch_half_open_and_recovery(self):
        world = w.World(config())
        for ms, valid in ((34999, True), (35000, False), (39999, False), (40000, True)):
            nav = world.frame(flight(), ms)["navigation"]
            self.assertEqual(nav["radios"][0]["identifier"]["value"] is not None, valid)

    def test_route_sequencing_and_remaining_distance(self):
        c = config(); c["faults"] = []
        world = w.World(c)
        start = world.frame(flight(), 0)["navigation"]["plan"]
        self.assertEqual(start["active_leg"], 0)
        self.assertGreater(start["remaining_route_nm"], start["distance_nm"])
        middle = c["route"][1]
        plan = world.frame(flight(middle["lat"], middle["lon"]), 50)["navigation"]["plan"]
        self.assertEqual(plan["active_leg"], 1)
        self.assertEqual(plan["waypoint"], "LIDB_REF")

    def test_direct_to_and_stationary_no_infinite_eta(self):
        c = config(); c["receiver"]["direct_to"] = "LIDB_REF"
        plan = w.World(c).frame(flight(gs=0), 0)["navigation"]["plan"]
        self.assertEqual(plan["direct_to"], "LIDB_REF")
        self.assertIsNone(plan["ete_to_waypoint_s"])
        self.assertIsNone(plan["eta_to_waypoint_utc"])

    def test_weather_products_delay_offline_and_expiry(self):
        world = w.World(config())
        good = world.weather_products(0)
        bad = world.weather_products(40000)
        offline = world.weather_products(45000)
        self.assertEqual(good["state"], "CURRENT")
        self.assertIn("RMK SIM TRAINING", good["raw_metar"])
        self.assertEqual(bad["state"], "EXPIRED")
        self.assertFalse(bad["display_allowed"])
        self.assertEqual(offline["transport"], "OFFLINE")
        self.assertNotEqual(offline["observation"], world.atmosphere(5000, 45000))
        self.assertFalse(good["safety_use_allowed"])

    def test_weather_boundaries_month_rollover(self):
        c = config(); c["epoch_utc"] = "2026-10-31T23:50:00Z"; c["faults"] = []
        world = w.World(c)
        product = world.weather_products(600000)
        self.assertIn("2026-11-01", product["observation_utc"])
        self.assertIn("ZZZZ 010000Z", product["raw_metar"])

    def test_future_weather_is_clock_skew_not_live(self):
        c = config()
        c["faults"] = [{"kind": "wx_clock_skew", "start_ms": 0, "end_ms": 1000, "target": "ALL"}]
        product = w.World(c).weather_products(0)
        self.assertEqual(product["state"], "CLOCK_SKEW")
        self.assertFalse(product["display_allowed"])
        self.assertLess(product["age_ms"], 0)
        c["faults"][0]["end_ms"] = 3600000
        self.assertEqual(w.World(c).weather_products(3599999)["state"], "CLOCK_SKEW")

    def test_radar_motion_georeference_png_and_limits(self):
        world = w.World(config())
        first, later = world.radar(0), world.radar(3600000)
        self.assertGreater(later["cell_lat"], first["cell_lat"])
        self.assertEqual(first["row_order"], "NORTH_TO_SOUTH")
        self.assertEqual(w.radar_png(first)[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(w.radar_png(first), w.radar_png(world.radar(0)))
        for size in (0, 129, True):
            with self.assertRaises(ValueError):
                world.radar(0, size)

    def test_clock_rewind_requires_reset_and_replay_determinism(self):
        a, b = w.World(config()), w.World(config())
        for ms in (0, 50, 20000, 35000, 60000):
            self.assertEqual(a.frame(flight(), ms), b.frame(flight(), ms))
        with self.assertRaises(ValueError):
            a.frame(flight(), 0)

    def test_weather_freshness_at_boundaries(self):
        for now, expected in ((600000, "CURRENT"), (600001, "STALE"),
                              (1200000, "STALE"), (1200001, "EXPIRED")):
            c = config()
            c["faults"] = [{"kind": "wx_offline", "start_ms": 0, "end_ms": 3600000, "target": "ALL"}]
            product = w.World(c).weather_products(now)
            self.assertEqual(product["state"], expected)

    def test_non_string_id_and_forecast_period_are_rejected_or_consistent(self):
        for key, value in (("dataset_id", 7), ("epoch_utc", None)):
            c = config(); c[key] = value
            with self.assertRaises(ValueError):
                w.World(c)
        p = w.World(config()).weather_products(600000)
        self.assertTrue(p["forecast_valid_from_utc"].endswith("10:00:00+00:00"))
        self.assertTrue(p["valid_to_utc"].endswith("16:00:00+00:00"))

    def test_session_config_rejected_before_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/"bad"
            c = config(); c["schema"] = "WRONG"
            with self.assertRaises(ValueError):
                s.run_session(path, seconds=1, world_config=c)
            self.assertFalse(path.exists())

    def test_replay_rejects_world_without_manifest_configuration(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            fixture(path, lambda f: f.update(world={"simulated": True}))
            with self.assertRaises(ValueError):
                s.verify_session(path)


if __name__ == "__main__":
    unittest.main()
