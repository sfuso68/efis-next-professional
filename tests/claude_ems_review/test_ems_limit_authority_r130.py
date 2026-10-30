"""Audit indipendente EMS R1-30 — stato osservato dell'autorita' limiti.

Appartiene alla review ``docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md``.

Questa suite fotografa il comportamento **attuale** del repository: fissa i
numeri esatti presenti nelle sorgenti e nei registri perche' l'integratore
possa accorgersi di una deriva. Passa oggi; se un valore cambia, il caso
fallisce e indica quale finding della review va rivisto.

Non modifica nulla: legge soltanto file. Non e' dichiarata in CMake, perche'
l'incarico vieta di toccare CMake/CI.

Esecuzione:
    python3 -m unittest discover -s tests/claude_ems_review -t . -v
Dipendenze: solo la libreria standard Python 3 (nessun Qt, nessun pytest).
"""

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

EMS_VIEW = ROOT / "qml/AdaptiveEmsView.qml"
DECK_VIEW = ROOT / "qml/ProfessionalSystemsDeck.qml"
TEMP_BARS = ROOT / "qml/EngineTemperatureBars.qml"
SETUP_OVERLAY = ROOT / "qml/EmsSetupOverlay.qml"
GOLDEN_CONTROLLER = ROOT / "qml/HmiGoldenController.qml"
EMS_CONTROLLER_H = ROOT / "src/nodes/ems/ems_controller.hpp"
EMS_CONTROLLER = ROOT / "src/nodes/ems/ems_controller.cpp"
PFD_CONTROLLER = ROOT / "src/renderers/qt/pfd_controller.cpp"
TYPES = ROOT / "include/efis/core/types.hpp"
SNAPSHOT = ROOT / "include/efis/engine/engine_snapshot.hpp"
EDP = ROOT / "src/protocol/edp_v1.cpp"
ACTIVE_PROFILE = ROOT / "config/active/aircraft-profile.json"
ENGINE_REGISTRY = ROOT / "config/product/engine-source-registry-v1.json"
REVIEW_GATE = ROOT / "config/product/engine-source-review-gate-v1.json"

GALLON_TO_LITRE = 3.78541
INHG_TO_HPA = 33.8639


def read(path):
    return path.read_text(encoding="utf-8")


def qml_fallback(text, function_name, limit_name):
    """Estrae il valore di ripiego usato da ``function_name("limit_name", X)``."""
    pattern = re.escape(function_name) + r'\(\s*"' + re.escape(limit_name) + r'"\s*,\s*([0-9.]+)\s*\)'
    found = {match.group(1) for match in re.finditer(pattern, text)}
    if len(found) != 1:
        raise AssertionError(
            f"{function_name}(\"{limit_name}\", ...) non ha un solo valore di ripiego: {sorted(found)}"
        )
    return float(found.pop())


class ProfileAndRegistryState(unittest.TestCase):
    """N1 — provenienza dei limiti nei registri."""

    def setUp(self):
        self.profile = json.loads(read(ACTIVE_PROFILE))
        self.engine = self.profile["engine"]
        self.limits = self.engine["limits"]

    def test_active_profile_declares_a_single_rotax_variant(self):
        self.assertEqual(self.engine["model"], "912 UL")
        self.assertEqual(self.profile["state"], "SIMULATION")

    def test_active_limits_are_declared_unverified(self):
        self.assertEqual(self.limits["status"], "INDICATIVE_UNVERIFIED")
        self.assertEqual(self.engine["manual_reference"], "TO_BE_VERIFIED")
        self.assertEqual(self.engine["serial_number"], "TO_BE_ENTERED")

    def test_registry_forbids_activation_for_both_variants(self):
        registry = json.loads(read(ENGINE_REGISTRY))
        self.assertFalse(registry["activation_allowed"])
        identifiers = [entry["id"] for entry in registry["engines"]]
        self.assertEqual(identifiers, ["ROTAX_912_UL", "ROTAX_912_ULS"])
        for entry in registry["engines"]:
            self.assertEqual(entry["review_state"], "NOT_REVIEWED")
            self.assertEqual(entry["limits"], [])
            self.assertFalse(entry["operational_use_allowed"])

    def test_review_gate_requires_ul_uls_separation_and_is_not_run(self):
        gate = json.loads(read(REVIEW_GATE))
        self.assertIn("ul-uls-separation", gate["required_findings"])
        self.assertIn("limit-page-traceability", gate["required_findings"])
        self.assertEqual(gate["reviewed_by"], [])
        self.assertFalse(gate["activation_allowed"])

    def test_coolant_is_a_required_sensor_without_a_declared_limit(self):
        sensors = {entry["signal"]: entry for entry in self.engine["sensors"]}
        self.assertIn("COOLANT_TEMPERATURE", sensors)
        self.assertTrue(sensors["COOLANT_TEMPERATURE"]["required"])
        self.assertNotIn("coolant_temperature_max_c", self.limits)

    def test_profile_declares_fuel_in_litres(self):
        self.assertEqual(self.profile["units"]["fuel"], "L")
        self.assertEqual(self.profile["systems"]["fuel"]["tank_capacity_l"], [40, 40])
        self.assertEqual(self.profile["aircraft"]["fuel_capacity_l"], 80)


class CoolantChannelAbsence(unittest.TestCase):
    """N2 — la temperatura liquido non esiste in nessuno strato."""

    def test_signal_vocabulary_has_no_coolant_signal(self):
        self.assertNotIn("coolant", read(TYPES).lower())

    def test_engine_snapshot_has_no_coolant_field(self):
        self.assertNotIn("coolant", read(SNAPSHOT).lower())

    def test_coolant_calibration_reads_the_oil_temperature_sample(self):
        for path in (EMS_CONTROLLER, PFD_CONTROLLER):
            text = read(path)
            coolant_lines = [line for line in text.splitlines() if "COOLANT_TEMPERATURE" in line]
            self.assertTrue(coolant_lines, f"{path} non menziona COOLANT_TEMPERATURE")
            aliased = [line for line in coolant_lines if "oil_temperature_c" in line]
            self.assertTrue(
                aliased,
                f"{path}: COOLANT_TEMPERATURE non e' piu' alias di oil_temperature_c",
            )


class UnitVocabulary(unittest.TestCase):
    """N3 — il vocabolario unita' non copre litro e bar."""

    def test_unit_enum_has_no_litre_and_no_bar(self):
        unit_block = read(TYPES)
        start = unit_block.index("enum class Unit")
        body = unit_block[start:unit_block.index("};", start)]
        self.assertIn("gallon", body)
        self.assertIn("pound_per_square_inch", body)
        self.assertNotIn("litre", body)
        self.assertNotIn("liter", body)
        self.assertNotIn("bar", body)

    def test_edp_pins_us_units_per_signal(self):
        text = read(EDP)
        start = text.index("Unit expected_unit(")
        body = text[start:text.index("\n}", start)]
        for unit in ("Unit::gallon_per_hour", "Unit::gallon",
                     "Unit::pound_per_square_inch", "Unit::inch_mercury"):
            self.assertIn(unit, body)


class LimitFallbacksDivergeBetweenDisplays(unittest.TestCase):
    """N4 — lo stesso limite ha due valori di ripiego diversi."""

    def setUp(self):
        self.ems = read(EMS_VIEW)
        self.deck = read(DECK_VIEW)

    def test_cht_fallback_is_230_on_the_ems_node(self):
        self.assertEqual(qml_fallback(self.ems, "engineLimit", "cht_max_c"), 230.0)

    def test_cht_fallback_is_240_on_the_pfd_canvas_deck(self):
        self.assertEqual(qml_fallback(self.deck, "limit", "cht_max_c"), 240.0)

    def test_temperature_bars_default_matches_the_deck_not_the_ems_node(self):
        match = re.search(r"property real chtMaximumC:\s*([0-9.]+)", read(TEMP_BARS))
        self.assertIsNotNone(match)
        self.assertEqual(float(match.group(1)), 240.0)

    def test_the_two_displays_disagree_by_ten_degrees(self):
        ems = qml_fallback(self.ems, "engineLimit", "cht_max_c")
        deck = qml_fallback(self.deck, "limit", "cht_max_c")
        self.assertEqual(deck - ems, 10.0)

    def test_every_other_limit_fallback_agrees(self):
        for name in ("rpm_max", "oil_temperature_max_c",
                     "oil_pressure_min_psi", "oil_pressure_max_psi", "egt_max_c"):
            with self.subTest(limit=name):
                try:
                    deck = qml_fallback(self.deck, "limit", name)
                except (AssertionError, ValueError):
                    self.skipTest(f"{name} non ha un ripiego nel deck")
                self.assertEqual(qml_fallback(self.ems, "engineLimit", name), deck)

    def test_rpm_fallback_is_far_below_the_declared_maximum(self):
        declared = json.loads(read(ACTIVE_PROFILE))["engine"]["rpm_max"]
        self.assertEqual(qml_fallback(self.ems, "engineLimit", "rpm_max"), 3000.0)
        self.assertEqual(declared, 5800)
        self.assertLess(qml_fallback(self.ems, "engineLimit", "rpm_max"), declared)


class ManifoldPressureScalesDiverge(unittest.TestCase):
    """N5 — la scala MAP non e' la stessa sui due display."""

    # Valori letti da qml/AdaptiveEmsView.qml e qml/ProfessionalSystemsDeck.qml.
    EMS_INHG = {"min": 0.0, "max": 40.0, "green_start": 5.0, "green_end": 34.0}
    DECK_INHG = {"min": 10.0, "max": 35.0, "green_start": 15.0, "green_end": 29.0}

    def test_the_literals_are_still_the_ones_this_audit_read(self):
        ems = read(EMS_VIEW)
        self.assertIn("maximum: root.metricUnits?1350:40", ems)
        self.assertIn("greenStart: root.metricUnits?170:5", ems)
        self.assertIn("greenEnd: root.metricUnits?1150:34", ems)
        deck = read(DECK_VIEW)
        self.assertIn("min:deck.metricUnits?340:10", deck)
        self.assertIn("max:deck.metricUnits?1185:35", deck)
        self.assertIn("gs:deck.metricUnits?508:15", deck)
        self.assertIn("ge:deck.metricUnits?982:29", deck)

    def test_a_value_green_on_the_ems_node_is_above_green_on_the_deck(self):
        value = 30.0  # inHg
        self.assertTrue(self.EMS_INHG["green_start"] <= value <= self.EMS_INHG["green_end"])
        self.assertGreater(value, self.DECK_INHG["green_end"])

    def test_a_value_green_on_the_ems_node_is_below_green_on_the_deck(self):
        value = 12.0  # inHg
        self.assertTrue(self.EMS_INHG["green_start"] <= value <= self.EMS_INHG["green_end"])
        self.assertLess(value, self.DECK_INHG["green_start"])

    def test_a_value_on_scale_for_the_ems_node_is_off_scale_on_the_deck(self):
        value = 5.0  # inHg
        self.assertGreaterEqual(value, self.EMS_INHG["min"])
        self.assertLess(value, self.DECK_INHG["min"])


class MetricLiteralsDriftFromTheirConversion(unittest.TestCase):
    """N6 — gli archi cambiano quando cambia il sistema di unita'."""

    def drift(self, imperial, declared_metric, factor):
        return abs(imperial * factor - declared_metric)

    def test_ems_manifold_arcs_drift(self):
        cases = {"max": (40.0, 1350.0), "green_start": (5.0, 170.0), "green_end": (34.0, 1150.0)}
        for name, (imperial, metric) in cases.items():
            with self.subTest(arc=name):
                self.assertGreater(self.drift(imperial, metric, INHG_TO_HPA), 0.5)

    def test_ems_fuel_flow_arcs_drift(self):
        cases = {"max": (30.0, 115.0), "green_start": (3.0, 11.0)}
        for name, (imperial, metric) in cases.items():
            with self.subTest(arc=name):
                self.assertGreater(self.drift(imperial, metric, GALLON_TO_LITRE), 0.3)

    def test_the_largest_manifold_drift_is_about_five_hectopascal(self):
        self.assertAlmostEqual(self.drift(40.0, 1350.0, INHG_TO_HPA), 4.556, places=2)

    def test_the_fuel_flow_maximum_drift_is_about_one_and_a_half_litre_per_hour(self):
        self.assertAlmostEqual(self.drift(30.0, 115.0, GALLON_TO_LITRE), 1.4377, places=3)


class HardCodedAircraftFacts(unittest.TestCase):
    """N7 — identita' motore e capacita' serbatoi scritte nel codice."""

    def test_tank_capacity_is_a_literal_in_the_ems_summary(self):
        self.assertIn("tankCapacity:root.fuel(20)", read(EMS_VIEW))

    def test_declared_capacity_does_not_match_that_literal(self):
        profile = json.loads(read(ACTIVE_PROFILE))
        per_tank_litre = profile["systems"]["fuel"]["tank_capacity_l"][0]
        per_tank_gallon = per_tank_litre / GALLON_TO_LITRE
        self.assertAlmostEqual(per_tank_gallon, 10.567, places=3)
        self.assertNotAlmostEqual(per_tank_gallon, 20.0, places=1)

    def test_setup_overlay_hard_codes_the_engine_rating(self):
        text = read(SETUP_OVERLAY)
        self.assertIn('s:"4 CYL • 80 HP • 5800 RPM"', text)
        self.assertIn('"ROTAX 912 UL"', text)
        self.assertIn("ROTAX 912 UL • MAINTENANCE PROGRAM", text)
        self.assertIn("PROFILE: ASSO-V-JOLLY-912-UL", text)

    def test_golden_fixture_limits_differ_from_the_active_profile(self):
        match = re.search(r"property var engineLimits:\(\{([^}]*)\}\)", read(GOLDEN_CONTROLLER))
        self.assertIsNotNone(match)
        fixture = {}
        for pair in match.group(1).split(","):
            key, _, value = pair.partition(":")
            fixture[key.strip()] = float(value)
        active = json.loads(read(ACTIVE_PROFILE))["engine"]["limits"]
        # Tre limiti su sei divergono fra la fixture golden e il profilo attivo.
        for key in ("cht_max_c", "oil_temperature_max_c", "oil_pressure_min_psi"):
            with self.subTest(limit=key):
                self.assertNotEqual(fixture[key], float(active[key]))
        # Solo il regime massimo coincide.
        self.assertEqual(fixture["rpm_max"], float(active["rpm_max"]))
        self.assertEqual(fixture["cht_max_c"], 135.0)
        self.assertEqual(float(active["cht_max_c"]), 230.0)
        self.assertEqual(fixture["oil_temperature_max_c"], 130.0)
        self.assertEqual(float(active["oil_temperature_max_c"]), 120.0)


class ProfileChangeIsNotPropagated(unittest.TestCase):
    """N8 — i limiti sono proprieta' CONSTANT in Qt."""

    def test_engine_identity_and_limits_are_constant_properties(self):
        text = read(EMS_CONTROLLER_H)
        for name in ("engineProfileName", "engineSerialNumber",
                     "engineLimits", "engineCapabilities"):
            with self.subTest(prop=name):
                match = re.search(r"Q_PROPERTY\([^)]*\b" + name + r"\b[^)]*\)", text)
                self.assertIsNotNone(match, f"{name} non e' una Q_PROPERTY")
                self.assertIn("CONSTANT", match.group(0))

    def test_an_operational_profile_can_still_be_selected_at_runtime(self):
        self.assertIn("Q_INVOKABLE bool selectOperationalProfile", read(EMS_CONTROLLER_H))

    def test_the_profile_is_loaded_once_from_the_constructor(self):
        text = read(EMS_CONTROLLER)
        self.assertEqual(text.count("loadAircraftProfile()"), 2)  # dichiarazione + unica chiamata


class LimitValidationAsymmetry(unittest.TestCase):
    """N9 — l'EMS non convalida i limiti che il PFD convalida."""

    def test_the_pfd_range_checks_staged_limits(self):
        text = read(PFD_CONTROLLER)
        self.assertIn("rpmMaximum >= 1000.0 && rpmMaximum <= 10000.0", text)
        self.assertIn("chtMaximum > 60.0 && chtMaximum <= 350.0", text)
        self.assertIn("INVALID_LIMITS", text)

    def test_the_ems_copies_the_limit_map_without_any_check(self):
        text = read(EMS_CONTROLLER)
        self.assertIn('engine_limits_ = engine.value("limits").toObject().toVariantMap();', text)
        start = text.index("void EmsController::loadAircraftProfile()")
        body = text[start:text.index("\nvoid EmsController::loadOperationalCatalog", start)]
        self.assertNotIn("INVALID", body)
        self.assertNotIn("qBound", body)

    def test_verified_limits_are_minted_from_a_four_character_reference(self):
        text = read(PFD_CONTROLLER)
        self.assertIn("manualReference.trimmed().size() >= 4", text)
        self.assertIn('{"state","VERIFIED_LIMITS"}', text)
        self.assertNotIn("engine-source-registry-v1.json", text)


class CalibrationIsNeverApplied(unittest.TestCase):
    """N10 — guadagno e offset si calcolano, si salvano e non si usano."""

    def test_gain_and_offset_are_computed_and_stored_but_never_applied(self):
        """Nessun sito di lettura converte un campione con il guadagno salvato."""
        for path in (EMS_CONTROLLER, PFD_CONTROLLER):
            dense = re.sub(r"\s+", "", read(path))
            with self.subTest(file=path.name):
                # Il guadagno viene calcolato...
                self.assertTrue(re.search(r'constdoublegain=spanReference/', dense),
                                f"{path.name}: il guadagno non viene piu' calcolato")
                # ...e scritto nelle impostazioni di calibrazione...
                self.assertTrue(re.search(r'setValue\(base\+"gain"', dense),
                                f"{path.name}: il guadagno non viene piu' persistito")
                # ...ma non moltiplica mai un valore misurato.
                # L'unico uso di `gain` a destra di una moltiplicazione e' il
                # calcolo dell'offset nella stessa riga che lo ha prodotto.
                without_offset = dense.replace('offset"),-zero.toDouble()*gain', "") \
                                      .replace('offset",-zero.toDouble()*gain', "")
                self.assertNotIn("*gain", without_offset,
                                 f"{path.name}: il guadagno ora viene applicato a un valore")
                self.assertNotIn("gain*", without_offset,
                                 f"{path.name}: il guadagno ora viene applicato a un valore")

    def test_calibration_revision_is_never_attached_to_a_sample(self):
        """``SampleOrigin.calibration_revision`` resta quello della sorgente."""
        self.assertIn("calibration_revision_", read(EMS_CONTROLLER))
        self.assertNotIn("origin.calibration_revision", read(EMS_CONTROLLER))

    def test_cylinder_calibration_only_ever_reads_the_first_cylinder(self):
        text = read(EMS_CONTROLLER)
        self.assertIn("cht_c.front()", text)
        self.assertIn("egt_c.front()", text)
        self.assertNotIn("cht_c[1]", text)

    def test_calibration_readiness_gates_nothing_but_a_label(self):
        consumers = [
            path for path in (ROOT / "qml").glob("*.qml")
            if "sensorCalibrationReady" in path.read_text(encoding="utf-8")
        ]
        self.assertEqual([path.name for path in consumers], ["EmsSetupOverlay.qml"])


class ElectricalThresholdsAreUnsourced(unittest.TestCase):
    """N11 — tre soglie elettriche diverse, nessuna dal profilo."""

    def test_three_distinct_bus_voltage_thresholds_exist(self):
        ems = read(EMS_VIEW)
        self.assertIn("controller.busVoltage<12.5", ems.replace(" ", ""))
        self.assertIn("busVoltage >= 13.2 && controller.busVoltage <= 14.8", ems)
        synoptic = read(ROOT / "qml/HmiElectricalSynoptic.qml")
        self.assertIn("busVoltage>=13.2&&busVoltage<=14.8", synoptic.replace(" ", ""))

    def test_the_profile_nominal_voltage_is_never_read(self):
        profile = json.loads(read(ACTIVE_PROFILE))
        self.assertEqual(profile["systems"]["electrical"]["nominal_voltage_v"], 12)
        for path in (EMS_VIEW, EMS_CONTROLLER, ROOT / "qml/HmiElectricalSynoptic.qml"):
            with self.subTest(file=path.name):
                self.assertNotIn("nominal_voltage_v", read(path))

    def test_bus_state_is_derived_from_the_bench_fault_selector(self):
        text = read(SETUP_OVERLAY)
        start = text.index("function busState(")
        body = text[start:text.index("\n", start)]
        self.assertIn("electricalTestMode", body)
        self.assertNotIn("busVoltage", body)


class FuelPressureHasNoConsumer(unittest.TestCase):
    """N12 — un canale trasportato end-to-end che nessuno mostra."""

    def test_the_channel_exists_in_every_layer(self):
        self.assertIn("fuel_pressure_psi", read(SNAPSHOT))
        self.assertIn("SignalId::fuel_pressure", read(EDP))
        self.assertIn("double fuelPressure()", read(EMS_CONTROLLER_H))
        self.assertIn("fuel_pressure_psi", read(ROOT / "src/providers/engine_simulation_provider.cpp"))

    def test_no_qml_view_displays_it(self):
        consumers = [
            path.name for path in (ROOT / "qml").glob("*.qml")
            if "fuelPressure" in path.read_text(encoding="utf-8")
        ]
        self.assertEqual(consumers, [])

    def test_the_ems_cas_has_no_fuel_pressure_branch(self):
        text = read(EMS_VIEW)
        start = text.index("readonly property string casHeadline")
        body = text[start:text.index("readonly property color casColor", start)]
        self.assertNotIn("FUEL PRESS", body)
        self.assertIn("LOW FUEL PRESS", read(EMS_CONTROLLER))  # esiste solo come stringa di guasto


if __name__ == "__main__":
    unittest.main()
