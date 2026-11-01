"""Audit indipendente EMS R1-30 — invarianti desiderati, oggi non soddisfatti.

Appartiene alla review ``docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md``.

A differenza di ``test_ems_limit_authority_r130.py``, che fotografa lo stato
attuale e passa, questa suite dichiara il comportamento che la review ritiene
corretto. **Oggi fallisce**, e ogni fallimento e' l'evidenza riproducibile di
un finding: il nome del caso contiene l'identificativo del finding.

Non e' un gate di regressione e non e' dichiarata in CMake: serve a misurare
la distanza dall'obiettivo e a diventare verde quando i gap sono chiusi.

Esecuzione:
    python3 tests/claude_ems_review/test_ems_gap_invariants_r130.py
Dipendenze: solo la libreria standard Python 3.
Exit code: 1 finche' i gap restano aperti; 0 quando sono tutti chiusi.
"""

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

EMS_VIEW = ROOT / "qml/AdaptiveEmsView.qml"
DECK_VIEW = ROOT / "qml/ProfessionalSystemsDeck.qml"
SETUP_OVERLAY = ROOT / "qml/EmsSetupOverlay.qml"
EMS_CONTROLLER_H = ROOT / "src/nodes/ems/ems_controller.hpp"
EMS_CONTROLLER = ROOT / "src/nodes/ems/ems_controller.cpp"
PFD_CONTROLLER = ROOT / "src/renderers/qt/pfd_controller.cpp"
TYPES = ROOT / "include/efis/core/types.hpp"
SNAPSHOT = ROOT / "include/efis/engine/engine_snapshot.hpp"
ACTIVE_PROFILE = ROOT / "config/active/aircraft-profile.json"

INHG_TO_HPA = 33.8639
GALLON_TO_LITRE = 3.78541


def read(path):
    return path.read_text(encoding="utf-8")


def fallback(text, function_name, limit_name):
    pattern = re.escape(function_name) + r'\(\s*"' + re.escape(limit_name) + r'"\s*,\s*([0-9.]+)\s*\)'
    values = {float(match.group(1)) for match in re.finditer(pattern, text)}
    return values


class CoolantTemperature(unittest.TestCase):
    """E-01 — il 912 UL/ULS e' raffreddato a liquido e il canale non esiste."""

    def test_e01a_the_signal_vocabulary_declares_a_coolant_temperature(self):
        self.assertIn("coolant", read(TYPES).lower())

    def test_e01b_the_engine_snapshot_carries_a_coolant_sample(self):
        self.assertIn("coolant", read(SNAPSHOT).lower())

    def test_e01c_coolant_calibration_does_not_read_the_oil_temperature(self):
        for path in (EMS_CONTROLLER, PFD_CONTROLLER):
            with self.subTest(file=path.name):
                for line in read(path).splitlines():
                    if "COOLANT_TEMPERATURE" in line:
                        self.assertNotIn("oil_temperature_c", line)


class LimitProvenance(unittest.TestCase):
    """E-02 — nessun limite inventato puo' raggiungere un arco."""

    def test_e02a_the_ems_view_has_no_invented_limit_fallbacks(self):
        text = read(EMS_VIEW)
        invented = {
            name: fallback(text, "engineLimit", name)
            for name in ("rpm_max", "cht_max_c", "egt_max_c",
                         "oil_temperature_max_c", "oil_pressure_min_psi",
                         "oil_pressure_max_psi")
        }
        self.assertEqual({name: values for name, values in invented.items() if values}, {})

    def test_e02b_the_systems_deck_has_no_invented_limit_fallbacks(self):
        text = read(DECK_VIEW)
        invented = {
            name: fallback(text, "limit", name)
            for name in ("rpm_max", "cht_max_c", "egt_max_c",
                         "oil_temperature_max_c", "oil_pressure_max_psi")
        }
        self.assertEqual({name: values for name, values in invented.items() if values}, {})

    def test_e02c_the_two_displays_cannot_disagree_on_a_limit(self):
        ems = fallback(read(EMS_VIEW), "engineLimit", "cht_max_c")
        deck = fallback(read(DECK_VIEW), "limit", "cht_max_c")
        self.assertEqual(ems, deck)

    def test_e02d_the_ems_validates_limits_before_presenting_them(self):
        text = read(EMS_CONTROLLER)
        start = text.index("void EmsController::loadAircraftProfile()")
        body = text[start:text.index("\nvoid EmsController::loadOperationalCatalog", start)]
        self.assertTrue(
            "INVALID" in body or "qBound" in body or "isfinite" in body,
            "loadAircraftProfile copia la mappa dei limiti senza alcun controllo",
        )

    def test_e02e_verified_limits_require_more_than_a_free_text_reference(self):
        text = read(PFD_CONTROLLER)
        self.assertNotIn("manualReference.trimmed().size() >= 4", text)

    def test_e02f_the_staging_path_consults_the_engine_source_registry(self):
        self.assertIn("engine-source-registry", read(PFD_CONTROLLER))


class ManifoldPressureScale(unittest.TestCase):
    """E-03 — la stessa grandezza deve avere la stessa scala sui due display."""

    def test_e03a_the_manifold_scale_is_not_duplicated_as_literals(self):
        self.assertNotIn("maximum: root.metricUnits?1350:40", read(EMS_VIEW))

    def test_e03b_the_two_displays_share_the_manifold_green_band(self):
        ems = re.search(r"greenStart: root\.metricUnits\?([0-9.]+):([0-9.]+).*?"
                        r"greenEnd: root\.metricUnits\?([0-9.]+):([0-9.]+)",
                        read(EMS_VIEW), re.S)
        deck = re.search(r"gs:deck\.metricUnits\?([0-9.]+):([0-9.]+),"
                         r"ge:deck\.metricUnits\?([0-9.]+):([0-9.]+)", read(DECK_VIEW))
        self.assertIsNotNone(ems, "la scala MAP dell'EMS non e' piu' un letterale")
        self.assertIsNotNone(deck, "la scala MAP del deck non e' piu' un letterale")
        ems_green = (float(ems.group(2)), float(ems.group(4)))
        deck_green = (float(deck.group(2)), float(deck.group(4)))
        self.assertEqual(ems_green, deck_green)


class UnitConsistency(unittest.TestCase):
    """E-04 — cambiare unita' non deve spostare i limiti."""

    # Solo gli estremi di scala e la fascia verde: il passo delle tacche puo'
    # legittimamente usare una griglia metrica arrotondata.
    ARC_PROPERTIES = ("minimum", "maximum", "greenStart", "greenEnd")

    def _arcs(self, label, prefix):
        gauge = re.search(r'label: "' + re.escape(label) + r'".*?limitsVerified',
                          read(EMS_VIEW), re.S)
        self.assertIsNotNone(gauge, f"il quadrante {label} non e' piu' riconoscibile")
        arcs = {}
        for name in self.ARC_PROPERTIES:
            match = re.search(re.escape(name) + r":\s*" + re.escape(prefix) +
                              r"metricUnits\?([0-9.]+):([0-9.]+)", gauge.group(0))
            if match:
                arcs[name] = (float(match.group(2)), float(match.group(1)))
        self.assertTrue(arcs, f"{label}: nessun arco e' piu' una coppia di letterali")
        return arcs

    def test_e04a_metric_manifold_arcs_are_the_exact_conversion(self):
        for name, (imperial, metric) in self._arcs("MAP", "root.").items():
            with self.subTest(arc=name, inhg=imperial, declared_hpa=metric):
                self.assertAlmostEqual(imperial * INHG_TO_HPA, metric, places=1)

    def test_e04b_metric_fuel_flow_arcs_are_the_exact_conversion(self):
        for name, (imperial, metric) in self._arcs("FUEL FLOW", "root.").items():
            with self.subTest(arc=name, gph=imperial, declared_lph=metric):
                self.assertAlmostEqual(imperial * GALLON_TO_LITRE, metric, places=1)

    def test_e04c_the_unit_vocabulary_covers_the_declared_profile_units(self):
        declared_fuel_unit = json.loads(read(ACTIVE_PROFILE))["units"]["fuel"]
        self.assertEqual(declared_fuel_unit, "L")
        self.assertIn("litre", read(TYPES).lower())


class TankCapacityAndIdentity(unittest.TestCase):
    """E-05 — capacita' e identita' motore vengono dal profilo, non dal codice."""

    def test_e05a_the_tank_capacity_is_not_a_literal(self):
        self.assertNotIn("tankCapacity:root.fuel(20)", read(EMS_VIEW))

    def test_e05b_the_setup_overlay_does_not_hard_code_the_engine_rating(self):
        text = read(SETUP_OVERLAY)
        self.assertNotIn('s:"4 CYL • 80 HP • 5800 RPM"', text)

    def test_e05c_the_setup_overlay_has_no_engine_identity_fallback(self):
        self.assertNotIn('"ROTAX 912 UL"', read(SETUP_OVERLAY))

    def test_e05d_the_setup_overlay_does_not_hard_code_the_profile_key(self):
        self.assertNotIn("PROFILE: ASSO-V-JOLLY-912-UL", read(SETUP_OVERLAY))


class ProfileChangePropagation(unittest.TestCase):
    """E-06 — un cambio di profilo deve aggiornare limiti e identita'."""

    def test_e06a_engine_limits_notify_a_change(self):
        text = read(EMS_CONTROLLER_H)
        match = re.search(r"Q_PROPERTY\([^)]*\bengineLimits\b[^)]*\)", text)
        self.assertIsNotNone(match)
        self.assertNotIn("CONSTANT", match.group(0))

    def test_e06b_engine_identity_notifies_a_change(self):
        text = read(EMS_CONTROLLER_H)
        for name in ("engineProfileName", "engineSerialNumber", "engineCapabilities"):
            with self.subTest(prop=name):
                match = re.search(r"Q_PROPERTY\([^)]*\b" + name + r"\b[^)]*\)", text)
                self.assertNotIn("CONSTANT", match.group(0))


class EngineHours(unittest.TestCase):
    """E-07 — contatori ore coerenti con profilo e motore installato."""

    def test_e07a_the_recovery_branch_keeps_the_profile_runtime_configuration(self):
        self.assertNotIn("RuntimeTracker{{500.0, 5000.0}, saved->totals}", read(EMS_CONTROLLER))

    def test_e07b_the_recorded_engine_serial_is_compared_on_recovery(self):
        text = read(EMS_CONTROLLER)
        start = text.index("if (const auto saved = runtime_store_->load()")
        self.assertIn("engine_serial", text[start:start + 400])

    def test_e07c_flight_hours_are_not_wired_to_a_constant_false(self):
        self.assertNotIn("runtime_tracker_.update(input.accountable_rpm, seconds, false)",
                         read(EMS_CONTROLLER))


class RunningThreshold(unittest.TestCase):
    """E-08 — una sola soglia di "motore in moto", dichiarata dal profilo."""

    def test_e08a_the_fuel_leak_monitor_does_not_use_a_local_literal(self):
        self.assertNotIn("engine_snapshot_.rpm.value<400.0", re.sub(r"\s+", "", read(EMS_CONTROLLER)))

    def test_e08b_the_derived_state_threshold_is_not_a_local_literal(self):
        self.assertNotIn("rpm.value > 400.0", read(ROOT / "src/core/derived_state.cpp"))

    def test_e08c_the_profile_threshold_is_the_only_one(self):
        declared = json.loads(read(ACTIVE_PROFILE))["engine"]["runtime_tracking"]["engine_run_threshold_rpm"]
        self.assertEqual(declared, 500)
        for path in (EMS_CONTROLLER, ROOT / "src/core/derived_state.cpp"):
            with self.subTest(file=path.name):
                self.assertNotIn("400.0", read(path))


class SimulationLabelling(unittest.TestCase):
    """E-09 — ADR-011: un valore simulato non si presenta come reale."""

    def test_e09a_the_ems_view_shows_the_sim_fallback_wording(self):
        self.assertIn("engineDataState", read(EMS_VIEW))

    def test_e09b_the_ems_view_distinguishes_lost_from_sim_fallback(self):
        text = read(EMS_VIEW)
        self.assertIn("SIM FALLBACK", text)


class PerChannelValidity(unittest.TestCase):
    """E-10 — ogni canale pubblica la propria validita' (chiuso su PR #56/#59)."""

    def test_e10a_the_controller_publishes_a_validity_per_channel(self):
        text = read(EMS_CONTROLLER_H)
        for name in ("engineRpmValid", "oilPressureValid", "oilTemperatureValid",
                     "fuelFlowValid", "busVoltageValid"):
            with self.subTest(prop=name):
                self.assertIn(name, text)

    def test_e10b_the_cylinder_arrays_publish_a_validity_too(self):
        self.assertIn("chtValid", read(EMS_CONTROLLER_H))


class ElectricalSynoptic(unittest.TestCase):
    """E-11 — lo stato delle barre deriva da una misura, non dal selettore banco."""

    def test_e11a_bus_state_reads_a_measured_voltage(self):
        text = read(SETUP_OVERLAY)
        start = text.index("function busState(")
        body = text[start:text.index("\n", start)]
        self.assertIn("busVoltage", body)

    def test_e11b_the_voltage_band_comes_from_the_profile(self):
        self.assertIn("nominal_voltage_v", read(EMS_VIEW))


class FuelPressureIndication(unittest.TestCase):
    """E-12 — un canale trasportato deve essere presentato o dichiarato assente."""

    def test_e12a_some_view_presents_the_fuel_pressure(self):
        consumers = [
            path.name for path in (ROOT / "qml").glob("*.qml")
            if "fuelPressure" in path.read_text(encoding="utf-8")
        ]
        self.assertNotEqual(consumers, [])

    def test_e12b_the_ems_cas_can_annunciate_low_fuel_pressure(self):
        text = read(EMS_VIEW)
        start = text.index("readonly property string casHeadline")
        body = text[start:text.index("readonly property color casColor", start)]
        self.assertIn("FUEL PRESS", body)


class CylinderCalibration(unittest.TestCase):
    """E-13 — quattro cilindri, quattro calibrazioni, applicate al valore."""

    def test_e13a_calibration_is_captured_per_cylinder(self):
        text = read(EMS_CONTROLLER)
        self.assertNotIn("cht_c.front()", text)

    def test_e13b_the_stored_gain_is_applied_to_the_indicated_value(self):
        dense = re.sub(r"\s+", "", read(EMS_CONTROLLER))
        without_offset = dense.replace('offset"),-zero.toDouble()*gain', "")
        self.assertIn("*gain", without_offset)

    def test_e13c_calibration_readiness_gates_more_than_a_label(self):
        consumers = [
            path.name for path in (ROOT / "qml").glob("*.qml")
            if "sensorCalibrationReady" in path.read_text(encoding="utf-8")
        ]
        self.assertGreater(len(consumers), 1)


class EnduranceComputation(unittest.TestCase):
    """E-14 — l'autonomia non si calcola dividendo per una portata inventata."""

    def test_e14a_the_endurance_does_not_floor_the_flow_at_a_tenth_of_a_gallon(self):
        self.assertNotIn("Math.max(.1,controller.fuelFlow)", re.sub(r"\s+", "", read(EMS_VIEW)))

    def test_e14b_there_is_a_single_endurance_formula(self):
        text = re.sub(r"\s+", "", read(EMS_VIEW))
        self.assertEqual(text.count("Autonomiastimata"), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
