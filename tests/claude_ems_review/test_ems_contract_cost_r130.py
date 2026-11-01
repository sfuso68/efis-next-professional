"""Costo del contratto dati EMS — evidenza della proposta di ADR R1-30.

Rende riproducibili i numeri della sezione 2 di
`docs/reviews/CLAUDE_EMS_COOLANT_AND_UNITS_ADR_PROPOSAL_R130.md`: la
disposizione congelata di EDP V1, il costo esatto di un canale aggiuntivo e il
fatto che l'enumerazione delle unità sia congelata dal validatore ICD.

Passa oggi. Se l'integratore revisiona l'ICD, i casi falliscono e indicano
quali cifre della proposta vanno riscritte.

Esecuzione:
    python3 tests/claude_ems_review/test_ems_contract_cost_r130.py
Dipendenze: solo la libreria standard Python 3.
Atteso: tutti i casi PASS, exit code 0.
"""

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ICD = ROOT / "config/protocols/avionix-data-icd-v1.json"
VECTORS = ROOT / "tests/data/protocols/avionix-data-icd-v1-vectors.json"
EDP_HEADER = ROOT / "include/efis/protocol/edp_v1.hpp"
EDP_SOURCE = ROOT / "src/protocol/edp_v1.cpp"
TYPES = ROOT / "include/efis/core/types.hpp"
ICD_VALIDATOR = ROOT / "tools/protocol/validate_avionix_icd.py"


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


class FrozenEdpLayout(unittest.TestCase):
    """2.1 — il contratto EDP V1 è congelato sui valori che la proposta cita."""

    def setUp(self):
        self.edp = load(ICD)["protocols"]["EDP_V1"]

    def test_the_icd_declares_the_layout_the_proposal_quotes(self):
        expected = {"schema_version": 1, "sample_count": 18, "sample_size": 36,
                    "payload_header_size": 4, "payload_size": 652, "envelope_size": 42}
        for key, value in expected.items():
            with self.subTest(field=key):
                self.assertEqual(self.edp[key], value)

    def test_the_edp_block_is_frozen(self):
        document = load(ICD)
        self.assertEqual(document["status"], "FROZEN-SW")
        self.assertEqual(document["frozen_utc"], "2026-09-17T00:00:00Z")

    def test_the_cpp_constants_match_the_icd(self):
        header = EDP_HEADER.read_text(encoding="utf-8")
        self.assertIn("engine_sample_count = 18", header)
        self.assertIn("envelope_size = 42", header)
        self.assertIn("schema_version = 1", header)
        source = EDP_SOURCE.read_text(encoding="utf-8")
        self.assertIn("sample_size = 36", source)
        self.assertIn("header_size = 4", source)


class AdditionalChannelCost(unittest.TestCase):
    """2.2 — un canale in più costa 36 byte e invalida il vettore congelato."""

    HEADER_BYTES = 4
    SAMPLE_BYTES = 36
    ENVELOPE_BYTES = 42

    def payload(self, samples):
        return self.HEADER_BYTES + samples * self.SAMPLE_BYTES

    def datagram(self, samples):
        return self.payload(samples) + self.ENVELOPE_BYTES

    def test_the_current_sizes_are_the_ones_quoted(self):
        self.assertEqual(self.payload(18), 652)
        self.assertEqual(self.datagram(18), 694)

    def test_a_nineteenth_sample_costs_thirty_six_bytes(self):
        self.assertEqual(self.payload(19), 688)
        self.assertEqual(self.datagram(19), 730)
        self.assertEqual(self.payload(19) - self.payload(18), self.SAMPLE_BYTES)

    def test_the_frozen_vector_pins_the_current_size_and_hashes(self):
        vector = load(VECTORS)["vectors"]["EDP_V1"]
        self.assertEqual(vector["wire_size"], self.datagram(18))
        self.assertEqual(vector["payload_crc32c"], "376ee061")
        self.assertEqual(
            vector["wire_sha256"],
            "d92e0a6e39d15c68b5b00866aa4dfbb35b465c7aa6856d1457710fa0f31c373c")

    def test_the_decoder_rejects_a_declared_count_that_differs(self):
        """Publisher e consumer non sono separabili sotto V1."""
        source = EDP_SOURCE.read_text(encoding="utf-8")
        self.assertIn("count != engine_sample_count", source)

    def test_the_envelope_already_carries_the_sample_count(self):
        """È il presupposto dell'opzione B: il conteggio è già sul filo."""
        source = EDP_SOURCE.read_text(encoding="utf-8")
        self.assertIn("u16(p, engine_sample_count)", source)
        header = load(ICD)["protocols"]["EDP_V1"]["payload_header"]
        names = [field[0] for field in header]
        self.assertIn("sample_count", names)


class UnitVocabularyIsFrozen(unittest.TestCase):
    """2.3 — sul filo c'è spazio, ma il gate congela l'enumerazione."""

    def test_the_unit_field_is_a_single_byte(self):
        sample = load(ICD)["protocols"]["EDP_V1"]["sample"]
        unit = [field for field in sample if field[0] == "unit"]
        self.assertEqual(len(unit), 1)
        name, offset, length, kind = unit[0]
        self.assertEqual((offset, length, kind), (14, 1, "u8"))

    def test_the_frozen_enumeration_has_exactly_twenty_values(self):
        units = load(ICD)["enumerations"]["unit"]
        self.assertEqual(len(units), 20)
        self.assertEqual(units["unknown"], 0)
        self.assertEqual(units["pascal"], 19)

    def test_the_validator_asserts_the_enumeration_is_unchanged(self):
        """Anche un'aggiunta in coda fallisce il gate finché l'ICD non cambia."""
        validator = ICD_VALIDATOR.read_text(encoding="utf-8")
        self.assertIn('spec.get("enumerations") != FROZEN_ENUMS', validator)

    def test_the_metric_units_the_proposal_asks_for_are_absent(self):
        units = load(ICD)["enumerations"]["unit"]
        for name in ("litre", "bar", "litre_per_hour"):
            with self.subTest(unit=name):
                self.assertNotIn(name, units)
        block = TYPES.read_text(encoding="utf-8")
        body = block[block.index("enum class Unit"):block.index("};", block.index("enum class Unit"))]
        for name in ("litre", "bar"):
            with self.subTest(unit=name):
                self.assertNotIn(name, body)

    def test_the_cpp_enum_and_the_icd_enumeration_agree(self):
        """Se divergessero, la proposta citerebbe un vocabolario inesistente."""
        block = TYPES.read_text(encoding="utf-8")
        start = block.index("enum class Unit")
        body = block[start:block.index("};", start)]
        # L'ultimo enumeratore non ha la virgola, e alcune righe sono commenti.
        lines = [line.split("//")[0].strip() for line in body.splitlines()[1:]]
        names = [re.match(r"(\w+)", line).group(1) for line in lines
                 if re.match(r"\w+\s*(?:=\s*\d+\s*)?,?$", line)]
        icd_units = load(ICD)["enumerations"]["unit"]
        self.assertEqual(len(names), len(icd_units))
        for position, name in enumerate(names):
            with self.subTest(unit=name):
                self.assertEqual(icd_units.get(name), position)


class ProposalStaysWithinItsAuthority(unittest.TestCase):
    """La proposta non deve introdurre un limite né assegnarsi un numero ADR."""

    PROPOSAL = ROOT / "docs/reviews/CLAUDE_EMS_COOLANT_AND_UNITS_ADR_PROPOSAL_R130.md"

    def test_the_document_declares_itself_a_proposal(self):
        text = self.PROPOSAL.read_text(encoding="utf-8")
        self.assertIn("proposta, non approvata e non eseguibile", text)

    def test_the_document_claims_no_adr_number(self):
        self.assertFalse(re.search(r"^# ADR-\d", self.PROPOSAL.read_text(encoding="utf-8"), re.M))

    def test_the_document_proposes_no_coolant_threshold(self):
        """Nessuna soglia: il registro motore dichiara le fonti UNRESOLVED."""
        text = self.PROPOSAL.read_text(encoding="utf-8")
        self.assertNotRegex(text, r"coolant\w*\s*(?:max|limit)\w*\s*[:=]\s*\d")
        self.assertNotRegex(text, r"\b1[0-9]{2}\s*°?\s*C\b")
        self.assertIn("UNRESOLVED", text)

    def test_the_engine_registry_still_blocks_activation(self):
        registry = load(ROOT / "config/product/engine-source-registry-v1.json")
        self.assertFalse(registry["activation_allowed"])
        self.assertEqual(registry["decision"], "NO_GO_UNTIL_APPLICABLE_SOURCES_REVIEWED")


if __name__ == "__main__":
    unittest.main()
