import copy
import json
import unittest
from pathlib import Path

from tools.ui.validate_hmi_contract import validate, validate_repository


ROOT = Path(__file__).resolve().parents[1]
BASE = json.loads((ROOT / "config/ui/avionix-hmi-v1.json").read_text(encoding="utf-8"))


class AvionixHmiContractTests(unittest.TestCase):
    def test_baseline_passes(self):
        self.assertEqual(validate(copy.deepcopy(BASE)), [])

    def test_batch20_all_contract_controls_fail_closed(self):
        mutations = []

        def case(control, mutate):
            data = copy.deepcopy(BASE)
            mutate(data)
            mutations.append((control, data))

        case(1, lambda data: data.update({"unexpected": True}))
        case(2, lambda data: data["logical_canvas"].update(width=1000))
        case(3, lambda data: data["targets"][0].update(platform="RPi4"))
        case(4, lambda data: data["targets"][0]["resolutions"].pop())
        case(5, lambda data: data["design_tokens"].update(minimum_stroke_px=1.0))
        case(6, lambda data: data["design_tokens"]["palette"].update(normal="#39d353"))
        case(7, lambda data: data["data_states"].remove("SIM"))
        case(8, lambda data: data["freshness_ms"].update(stale_max=999))
        case(9, lambda data: data["skyview"]["zones"].reverse())
        case(10, lambda data: data["skyview"]["v_speed_labels"].reverse())
        case(11, lambda data: data["skyview"].update(hsi_visible_half_angle_deg=45))
        case(12, lambda data: data["skyview"]["terrain"].update(age_required=False))
        case(13, lambda data: data["skyview"]["overlays"].reverse())
        case(14, lambda data: data["skyview"].update(target_fps=float("nan")))
        case(15, lambda data: data["ems"]["compact_mode"].update(minimum_micro_text_px=6))
        case(16, lambda data: data["ems"]["secondary_gauges"].reverse())
        case(17, lambda data: data["ems"]["cylinder_channels"].reverse())
        case(18, lambda data: data["ems"]["flap_detents_deg"].pop())
        case(19, lambda data: data["ems"]["pages"].remove("SETTINGS"))
        case(20, lambda data: data["degraded_mode"].update(no_control_authority=False))

        self.assertEqual(len(mutations), 20)
        for control, data in mutations:
            with self.subTest(control=control):
                marker = f"[HMI-B20-{control:02d}]"
                self.assertTrue(any(error.startswith(marker) for error in validate(data)))

    def test_non_object_contract_fails_closed_without_exception(self):
        self.assertTrue(validate([]))

    def test_repository_components_are_registered(self):
        self.assertEqual(validate_repository(ROOT), [])

    def test_platform_swap_fails(self):
        data = copy.deepcopy(BASE)
        data["targets"][0]["platform"] = "RPi4"
        self.assertTrue(validate(data))

    def test_missing_sim_state_fails(self):
        data = copy.deepcopy(BASE)
        data["data_states"].remove("SIM")
        self.assertTrue(validate(data))

    def test_small_touch_target_fails(self):
        data = copy.deepcopy(BASE)
        data["design_tokens"]["minimum_touch_px"] = 32
        self.assertTrue(validate(data))

    def test_missing_vne_fails(self):
        data = copy.deepcopy(BASE)
        data["skyview"]["v_speed_labels"].remove("VNE")
        self.assertTrue(validate(data))

    def test_command_authority_fails(self):
        data = copy.deepcopy(BASE)
        data["degraded_mode"]["no_control_authority"] = False
        self.assertTrue(validate(data))

    def test_compact_mode_text_floor_fails_closed(self):
        data = copy.deepcopy(BASE)
        data["ems"]["compact_mode"]["minimum_micro_text_px"] = 6
        self.assertTrue(validate(data))

    def test_compact_mode_touch_floor_fails_closed(self):
        data = copy.deepcopy(BASE)
        data["ems"]["compact_mode"]["minimum_touch_px"] = 36
        self.assertTrue(validate(data))

    def test_degraded_state_is_wired_to_gauges_and_age(self):
        ems = (ROOT / "qml/AdaptiveEmsView.qml").read_text(encoding="utf-8")
        gauge = (ROOT / "qml/HmiArcGaugeV4.qml").read_text(encoding="utf-8")
        face = (ROOT / "qml/EmsGaugeV3.qml").read_text(encoding="utf-8")
        self.assertIn("dataAgeMs:root.dataAgeMs", ems)
        self.assertIn("normalRangesVisible: gauge.usable", gauge)
        self.assertIn('arc(gauge.minimum,gauge.maximum,"#8A96A3"', face)

    def test_golden_fixture_accepts_explicit_state_and_age(self):
        fixture = (ROOT / "qml/HmiGoldenController.qml").read_text(encoding="utf-8")
        main = (ROOT / "src/renderers/qt/main.cpp").read_text(encoding="utf-8")
        self.assertIn("hmiGoldenDataState", fixture)
        self.assertIn("hmiGoldenDataAgeMs", fixture)
        self.assertIn("EFIS_HMI_GOLDEN_STATE", main)
        self.assertIn("EFIS_HMI_GOLDEN_AGE_MS", main)

    def test_invalid_data_blocks_derived_ems_functions(self):
        ems = (ROOT / "qml/AdaptiveEmsView.qml").read_text(encoding="utf-8")
        self.assertIn('readonly property bool dataUsable:', ems)
        self.assertIn('!dataUsable ? "WARNING"', ems)
        self.assertIn('if(!dataUsable)return; resetLean()', ems)
        self.assertIn('if(!root.dataUsable){root.leanActive=false;root.updateCasEvents();return}', ems)
        self.assertIn('checklistEngineOk: dataUsable &&', ems)

    def test_secondary_modules_receive_age(self):
        ems = (ROOT / "qml/AdaptiveEmsView.qml").read_text(encoding="utf-8")
        for component in ("HmiCylinderMatrixV2", "HmiAirframePanel",
                          "HmiFuelStatus", "HmiElectricalSynoptic"):
            start = ems.index(component)
            self.assertIn("dataAgeMs:root.dataAgeMs", ems[start:start + 900])

    def test_operational_ems_exposes_edp_state_and_age(self):
        header = (ROOT / "src/nodes/ems/ems_controller.hpp").read_text(encoding="utf-8")
        source = (ROOT / "src/nodes/ems/ems_controller.cpp").read_text(encoding="utf-8")
        self.assertIn("Q_PROPERTY(int dataAgeMs READ dataAgeMs", header)
        self.assertIn("engine_network_source_->link_status(now)", source)
        self.assertNotIn("engine_network_source_->data_age(now)", source)
        for state in ("LIVE", "STALE", "LOST", "SIM"):
            self.assertIn(f'QStringLiteral("{state}")', source)
        self.assertNotIn('data_source_state_ = QStringLiteral("NETWORK")', source)
        self.assertNotIn('QStringLiteral("SIMULATION")', header)

    def test_ems_setup_exposes_read_only_edp_diagnostics(self):
        header = (ROOT / "src/nodes/ems/ems_controller.hpp").read_text(encoding="utf-8")
        source = (ROOT / "src/nodes/ems/ems_controller.cpp").read_text(encoding="utf-8")
        setup = (ROOT / "qml/EmsSetupOverlay.qml").read_text(encoding="utf-8")
        main = (ROOT / "qml/EmsMain.qml").read_text(encoding="utf-8")
        self.assertIn("Q_PROPERTY(QVariantMap engineLinkDiagnostics", header)
        self.assertIn("event=ems_edp_link_transition", source)
        for metric in ("crcRejected", "identityRejected", "sequenceGaps",
                       "publisherRestarts", "sourceNode", "publisherInstance",
                       "sequence", "lastRejection", "lastRejectionAgeMs",
                       "validPercent", "sessionStatus", "recentRejection",
                       "expectedSourceNode", "authorityStatus",
                       "staleAfterMs", "lostAfterMs"):
            self.assertIn(metric, source)
            self.assertIn(metric, setup)
        self.assertIn('"DATA LINK"', setup)
        self.assertIn("READ-ONLY DIAGNOSTICS", setup)
        self.assertIn("height:rail.height/7", setup)
        self.assertIn("visible:setup.section===6", setup)
        self.assertIn('indexOf("--setup-edp-preview")', main)
        self.assertIn("if (edpPreview) setupSection = 5", main)
        self.assertIn("if (operationalPreview) setupSection = 6", main)

        link_page = setup[setup.index("Item { id:linkPage"):setup.index(
            "OperationalProfileSetup", setup.index("Item { id:linkPage"))]
        self.assertIn("ACTIVE", link_page)
        self.assertIn("PUBLISHER", link_page)
        self.assertIn("LAST REJECT", link_page)
        self.assertIn("AUTHORITY", link_page)
        self.assertIn("STALE/LOST", link_page)
        self.assertIn("RECENT", link_page)
        self.assertNotIn("TapHandler", link_page)
        self.assertNotIn("onClicked", link_page)


if __name__ == "__main__":
    unittest.main()
