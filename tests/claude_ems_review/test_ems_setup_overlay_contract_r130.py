"""Lotto L0 dell'audit EMS R1-30 — identità del velivolo nella pagina di setup.

Verifica l'implementazione del finding E-05 su `qml/EmsSetupOverlay.qml`, il
solo file assegnato in esclusiva a questo lotto perché è l'unico della
superficie EMS che lo stack aperto #52/#53/#56/#59 non modifica.

Senza Qt non si può istanziare il QML, quindi la verifica è statica e copre tre
cose: che nessuna identità di motore o velivolo resti cablata, che ogni
riferimento al controller risolva davvero a una sua proprietà o funzione
invocabile, e che il contratto DATA LINK su cui si appoggia
`tests/test_avionix_hmi_contract.py` sia rimasto intatto.

Esecuzione:
    python3 tests/claude_ems_review/test_ems_setup_overlay_contract_r130.py
Dipendenze: solo la libreria standard Python 3.
Atteso: tutti i casi PASS, exit code 0.
"""

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OVERLAY = ROOT / "qml/EmsSetupOverlay.qml"
CONTROLLER_H = ROOT / "src/nodes/ems/ems_controller.hpp"
ACTIVE_PROFILE = ROOT / "config/active/aircraft-profile.json"

SOURCE = OVERLAY.read_text(encoding="utf-8")
HEADER = CONTROLLER_H.read_text(encoding="utf-8")

# I commenti spiegano perché un nome di motore non va cablato, quindi lo
# contengono legittimamente: i controlli sul testo mostrato li escludono.
CODE = "\n".join(line.split("//")[0] for line in SOURCE.splitlines())


def strip_strings_and_comments(text):
    """Rimuove letterali di stringa e commenti di riga, per i conteggi."""
    out, index, size = [], 0, len(text)
    while index < size:
        char = text[index]
        if char == '"':
            index += 1
            while index < size and text[index] != '"':
                index += 2 if text[index] == "\\" else 1
            index += 1
            continue
        if text.startswith("//", index):
            while index < size and text[index] != "\n":
                index += 1
            continue
        out.append(char)
        index += 1
    return "".join(out)


class NoHardCodedIdentity(unittest.TestCase):
    """E-05 — nessun motore e nessun velivolo nominato dal codice."""

    FORBIDDEN = (
        "4 CYL • 80 HP • 5800 RPM",
        "ASSO V JOLLY",
        "ASSO-V-JOLLY-912-UL",
        "PIONEER 300 EQUIVALENT",
        "OWNER BUILD",
        "2 × 40 L",
        "RETRACT GEAR • FLAPS",
        "0 / 10 / 15 / 20 / 30°",
    )

    def test_no_forbidden_literal_survives(self):
        for literal in self.FORBIDDEN:
            with self.subTest(literal=literal):
                self.assertFalse(literal in CODE, "letterale ancora presente")

    def test_no_engine_model_is_named_in_displayed_text(self):
        for name in ("ROTAX", "Rotax", "912 UL", "912 ULS"):
            with self.subTest(name=name):
                self.assertFalse(name in CODE, "nome motore cablato nel codice")

    def test_the_engine_identity_comes_from_the_controller(self):
        self.assertIn('{k:"ENGINE",v:setup.textOr("engineProfileName")', SOURCE)
        self.assertIn('{k:"ENGINE SERIAL",v:setup.textOr("engineSerialNumber")', SOURCE)

    def test_the_maintenance_header_comes_from_the_controller(self):
        self.assertIn('text:setup.textOr("engineProfileName")+" • MAINTENANCE PROGRAM"', SOURCE)

    def test_the_maintenance_note_does_not_assume_the_manufacturer(self):
        self.assertIn("the applicable engine maintenance manual", SOURCE)
        self.assertNotIn("Rotax maintenance manual", SOURCE)

    def test_the_footer_shows_the_profile_the_controller_publishes(self):
        self.assertIn('text:"OPERATIONAL PROFILE: "+setup.textOr("operationalSelectedProfile")', SOURCE)


class AbsenceIsDeclared(unittest.TestCase):
    """ADR-004 — un campo non pubblicato si dichiara, non si sostituisce."""

    def test_the_absence_marker_exists_and_is_a_single_definition(self):
        matches = re.findall(r'readonly property string unpublished:\s*"([^"]+)"', SOURCE)
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0], "NOT PUBLISHED BY PROFILE")

    def test_the_fields_the_controller_cannot_supply_declare_absence(self):
        for field in ("AIRFRAME", "CONFIGURATION", "FUEL"):
            with self.subTest(field=field):
                self.assertIn('{k:"%s",v:setup.unpublished' % field, SOURCE)

    def test_a_blank_controller_string_is_treated_as_absent(self):
        """Un profilo illeggibile produce un nome di un solo spazio: va trimmato.

        `loadAircraftProfile` compone il nome come manufacturer + " " + model, e
        su un JSON non analizzabile entrambi sono vuoti.
        """
        match = re.search(r"function textOr\(name\) \{([^\n]+)\}", SOURCE)
        self.assertIsNotNone(match)
        body = match.group(1)
        self.assertIn(".trim()", body)
        self.assertIn("setup.unpublished", body)

    def test_cylinders_and_rated_power_are_declared_absent(self):
        match = re.search(r"function engineRating\(\) \{(.*?)\n    \}", SOURCE, re.S)
        self.assertIsNotNone(match)
        body = match.group(1)
        self.assertIn("CYL / POWER NOT PUBLISHED", body)
        # Nessuna cifra di potenza o di cilindri viene dedotta.
        self.assertNotIn("80", body)
        self.assertNotIn("100", body)
        self.assertNotIn("4 CYL", body)


class IntroducedLabelsAreBounded(unittest.TestCase):
    """L'audit vieta etichette non vincolate: non va introdotta una nuova."""

    def test_the_installation_delegate_bounds_value_and_detail(self):
        delegate = re.search(r"delegate:Rectangle \{ required property var modelData; "
                             r"width:385\*setup\.uiScale.*?\n                \}",
                             SOURCE, re.S)
        self.assertIsNotNone(delegate, "delegate della griglia non riconoscibile")
        body = delegate.group(0)
        for field in ("modelData.v", "modelData.s"):
            with self.subTest(field=field):
                line = [l for l in body.splitlines() if f"text:{field}" in l]
                self.assertTrue(line, f"{field} non trovato")
                self.assertIn("elide:Text.ElideRight", line[0])
                self.assertIn("width:", line[0])

    def test_the_rating_label_stays_short(self):
        """Il testo peggiore deve stare nella larghezza del riquadro."""
        worst = "CYL / POWER NOT PUBLISHED • 5800 RPM MAX • INDICATIVE_UNVERIFIED"
        self.assertLessEqual(len(worst), 70)

    def test_the_maintenance_note_is_no_longer_than_before(self):
        note = ("Intervals must be confirmed against the applicable engine maintenance "
                "manual, serial number, fuel and operating conditions.")
        previous = ("Intervals must be confirmed against the applicable Rotax maintenance "
                    "manual, engine serial number, fuel and operating conditions.")
        self.assertIn(note, SOURCE)
        self.assertLessEqual(len(note), len(previous))


class LimitProvenanceIsCarried(unittest.TestCase):
    """E-02, parte raggiungibile da questo lotto."""

    def test_the_rated_rpm_is_never_shown_without_its_status(self):
        match = re.search(r"function engineRating\(\) \{(.*?)\n    \}", SOURCE, re.S)
        body = match.group(1)
        self.assertIn("RPM MAX", body)
        self.assertIn("setup.limitStatus()", body)

    def test_an_absent_rated_rpm_is_declared_absent(self):
        body = re.search(r"function engineRating\(\) \{(.*?)\n    \}", SOURCE, re.S).group(1)
        self.assertIn("RPM MAX NOT PUBLISHED", body)

    def test_the_rated_rpm_has_no_fallback_number(self):
        body = re.search(r"function engineRating\(\) \{(.*?)\n    \}", SOURCE, re.S).group(1)
        rpm_line = [line for line in body.splitlines() if "rpm_max" in line]
        self.assertTrue(rpm_line)
        self.assertNotRegex(rpm_line[0], r"rpm_max\s*\|\|\s*[0-9]")

    def test_the_declared_profile_status_is_still_unverified(self):
        """Il profilo attivo non è verificato: la pagina deve poterlo dire."""
        status = json.loads(ACTIVE_PROFILE.read_text(encoding="utf-8"))["engine"]["limits"]["status"]
        self.assertEqual(status, "INDICATIVE_UNVERIFIED")
        self.assertIn("function limitStatus()", SOURCE)


class AirframeSystemsFromTheProfile(unittest.TestCase):
    """E-05 — carrello, flap e detent derivano dalle proprietà pubblicate."""

    def test_airframe_systems_reads_the_installation_flags(self):
        body = re.search(r"function airframeSystems\(\) \{(.*?)\n    \}", SOURCE, re.S).group(1)
        for prop in ("landingGearInstalled", "retractableGear", "flapsInstalled"):
            with self.subTest(prop=prop):
                self.assertIn(prop, body)
        self.assertIn("setup.unpublished", body)

    def test_flap_detents_are_read_from_the_published_list(self):
        body = re.search(r"function flapDetentText\(\) \{(.*?)\n    \}", SOURCE, re.S).group(1)
        self.assertIn('setup.listOr("flapDetents")', body)
        self.assertIn("isFinite", body)
        self.assertIn("setup.unpublished", body)

    def test_the_profile_still_declares_the_detents_this_reads(self):
        flaps = json.loads(ACTIVE_PROFILE.read_text(encoding="utf-8"))["systems"]["flaps"]
        self.assertEqual(flaps["detents_deg"], [0, 10, 15, 20, 30])


class ControllerReferencesResolve(unittest.TestCase):
    """Nessun riferimento al controller può puntare a una proprietà inesistente."""

    @staticmethod
    def controller_surface():
        properties = set(re.findall(r"Q_PROPERTY\([A-Za-z<>:]+\s+(\w+)", HEADER))
        invokables = set(re.findall(r"Q_INVOKABLE\s+[\w:]+\s+(\w+)", HEADER))
        return properties | invokables

    def test_every_direct_reference_exists_on_the_controller(self):
        used = set(re.findall(r"controller\.(\w+)", SOURCE))
        unknown = sorted(used - self.controller_surface())
        self.assertEqual(unknown, [], f"riferimenti inesistenti: {unknown}")

    def test_every_accessor_helper_argument_exists_on_the_controller(self):
        used = set(re.findall(r'(?:textOr|listOr|mapOr|numberOr)\("(\w+)"\)', SOURCE))
        self.assertTrue(used, "nessun accesso per nome trovato")
        unknown = sorted(used - self.controller_surface())
        self.assertEqual(unknown, [], f"riferimenti inesistenti: {unknown}")

    def test_every_setup_reference_is_defined_in_the_file(self):
        defined = set(re.findall(r"function\s+(\w+)\s*\(", SOURCE))
        defined |= set(re.findall(r"property\s+[\w<>]+\s+(\w+)", SOURCE))
        defined |= set(re.findall(r"signal\s+(\w+)\s*\(", SOURCE))
        defined |= set(re.findall(r"id:\s*(\w+)", SOURCE))
        unknown = sorted(set(re.findall(r"setup\.(\w+)", SOURCE)) - defined)
        self.assertEqual(unknown, [], f"riferimenti setup.* non definiti: {unknown}")

    def test_the_new_helpers_are_actually_used(self):
        for helper in ("textOr", "engineRating", "airframeSystems",
                       "flapDetentText", "limitStatus"):
            with self.subTest(helper=helper):
                self.assertGreaterEqual(SOURCE.count(f"setup.{helper}("), 1)


class SyntaxStillBalanced(unittest.TestCase):
    """Un QML sbilanciato non si carica, e senza Qt non lo vedremmo altrimenti."""

    def test_delimiters_balance(self):
        clean = strip_strings_and_comments(SOURCE)
        for opening, closing in (("{", "}"), ("(", ")"), ("[", "]")):
            with self.subTest(pair=opening + closing):
                self.assertEqual(clean.count(opening), clean.count(closing))

    def test_the_file_is_still_a_single_rectangle_root(self):
        self.assertTrue(SOURCE.lstrip().startswith("import QtQuick"))
        self.assertIn("Rectangle {\n    id: setup", SOURCE)


class ExistingContractsPreserved(unittest.TestCase):
    """Il lotto non deve rompere i test che non possiede."""

    # tests/test_avionix_hmi_contract.py::test_ems_setup_exposes_read_only_edp_diagnostics
    EDP_METRICS = ("crcRejected", "identityRejected", "sequenceGaps",
                   "publisherRestarts", "sourceNode", "publisherInstance",
                   "sequence", "lastRejection", "lastRejectionAgeMs",
                   "validPercent", "sessionStatus", "recentRejection",
                   "expectedSourceNode", "authorityStatus",
                   "staleAfterMs", "lostAfterMs")

    def test_the_data_link_diagnostics_contract_is_intact(self):
        for metric in self.EDP_METRICS:
            with self.subTest(metric=metric):
                self.assertIn(metric, SOURCE)
        self.assertIn('"DATA LINK"', SOURCE)

    def test_the_seven_setup_sections_are_unchanged(self):
        self.assertIn('model:["ENGINE","HOURS","SENSORS","ELECTRICAL",'
                      '"LOG & SERVICE","DATA LINK","AIRCRAFT DATA"]', SOURCE)


if __name__ == "__main__":
    unittest.main()
