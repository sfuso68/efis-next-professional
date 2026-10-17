import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
import zlib

from tools.ui.capture_hmi_golden import (
    SIZES_BY_ROLE, png_pixel_sha256, png_size, png_pixels,
    write_manifest as build_manifest,
)
from tools.ui.compare_hmi_golden import difference
from tools.ui.validate_hmi_golden import validate


def fake_png(path: Path, width: int, height: int, total_bytes: int = 12000) -> None:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))
    signature = b"\x89PNG\r\n\x1a\n"
    ihdr = chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    rows = b"".join(b"\x00" + bytes(width * 4) for _ in range(height))
    idat = chunk(b"IDAT", zlib.compress(rows))
    padding = chunk(b"tEXt", b"fixture=" + b"x" * total_bytes)
    path.write_bytes(signature + ihdr + padding + idat + chunk(b"IEND", b""))


def fake_bundle(directory: Path, role: str = "ems") -> dict:
    images = []
    for width, height in SIZES_BY_ROLE[role]:
        path = directory / f"{role}-{width}x{height}.png"
        fake_png(path, width, height)
        images.append({"file": path.name, "width": width, "height": height,
                       "bytes": path.stat().st_size,
                       "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                       "pixel_sha256": png_pixel_sha256(path)})
    return {
        "schema": "avionix.hmi-golden-manifest.v2",
        "status": "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE",
        "generated_utc": "2026-09-17T20:00:00+00:00",
        "platform": "test",
        "fixture": "SkyViewGoldenMain.qml" if role == "skyview" else "HmiGoldenMain.qml",
        "role": role,
        "data_state": "SIM",
        "data_age_ms": 0,
        "images": images,
    }


def write_manifest(directory: Path, manifest: dict) -> None:
    (directory / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")


class HmiGoldenToolsTests(unittest.TestCase):
    def test_none_fast_path_preserves_previous_row_for_up_filter(self):
        def chunk(kind, payload):
            body = kind + payload
            return struct.pack('>I', len(payload)) + body + struct.pack('>I', zlib.crc32(body))
        header = chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 2, 8, 6, 0, 0, 0))
        pixels = bytes([10, 20, 30, 40, 11, 22, 33, 44])
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'mixed.png'
            rows = b'\x00' + pixels[:4] + b'\x02' + bytes([1, 2, 3, 4])
            path.write_bytes(b'\x89PNG\r\n\x1a\n' + header + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
            self.assertEqual(pixels, png_pixels(path))

    def test_visual_difference_has_explicit_tolerance_metrics(self):
        self.assertEqual(difference(bytes([1, 2, 3]), bytes([1, 4, 3])), (2, 2/3, 1/3))

    def test_png_dimensions(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "image.png"
            fake_png(path, 1024, 600)
            self.assertEqual(png_size(path), (1024, 600))
            self.assertEqual(len(png_pixel_sha256(path)), 64)

    def test_complete_manifest_passes_and_tamper_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            manifest = fake_bundle(directory)
            write_manifest(directory, manifest)
            self.assertEqual(validate(directory), [])
            (directory / "ems-800x480.png").write_bytes(b"tampered")
            self.assertTrue(validate(directory))

    def test_skyview_matrix_includes_full_hd(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            manifest = fake_bundle(directory, "skyview")
            write_manifest(directory, manifest)
            self.assertEqual(validate(directory), [])
            self.assertIn((1920, 1080), SIZES_BY_ROLE["skyview"])

    def test_degraded_is_a_canonical_golden_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            manifest = fake_bundle(directory)
            manifest["data_state"] = "DEGRADED"
            manifest["data_age_ms"] = 600
            write_manifest(directory, manifest)
            self.assertEqual(validate(directory), [])

    def test_manifest_can_be_built_from_direct_captures(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            fake_bundle(directory, "skyview")
            manifest = build_manifest(directory, "test", "SIM", 0, "skyview")
            self.assertEqual(manifest["role"], "skyview")
            self.assertEqual(validate(directory), [])

    def test_batch20_all_golden_controls_fail_closed(self):
        def mutate(control, manifest, directory):
            if control == 1: manifest["schema"] = "unknown"
            elif control == 2: manifest["unknown"] = True
            elif control == 3: manifest["status"] = "HARDWARE_ACCEPTED"
            elif control == 4: manifest["role"] = "pfd"
            elif control == 5: manifest["fixture"] = "Other.qml"
            elif control == 6: manifest["platform"] = ""
            elif control == 7: manifest["generated_utc"] = "2026-09-17T20:00:00"
            elif control == 8: manifest["data_state"] = "UNKNOWN"
            elif control == 9: manifest["data_age_ms"] = -1
            elif control == 10: manifest["images"].pop()
            elif control == 11: manifest["images"][0]["unknown"] = True
            elif control == 12: manifest["images"][0]["file"] = "wrong.png"
            elif control == 13: manifest["images"][1]["file"] = manifest["images"][0]["file"]
            elif control == 14: (directory / manifest["images"][0]["file"]).unlink()
            elif control == 15: fake_png(directory / manifest["images"][0]["file"], 2, 2)
            elif control == 16: manifest["images"][0]["bytes"] += 1
            elif control == 17: manifest["images"][0]["sha256"] = "0" * 64
            elif control == 18: manifest["images"][0]["pixel_sha256"] = "0" * 64
            elif control == 19: manifest["images"][0]["file"] = "../escape.png"
            elif control == 20: manifest["images"].reverse()

        with tempfile.TemporaryDirectory() as baseline_temporary:
            baseline = Path(baseline_temporary)
            baseline_manifest = fake_bundle(baseline, "skyview")
            for control in range(1, 21):
                with self.subTest(control=control), tempfile.TemporaryDirectory() as temporary:
                    directory = Path(temporary)
                    for image in baseline_manifest["images"]:
                        shutil.copy2(baseline / image["file"], directory / image["file"])
                    manifest = copy.deepcopy(baseline_manifest)
                    mutate(control, manifest, directory)
                    write_manifest(directory, manifest)
                    marker = f"[HMI-G20-{control:02d}]"
                    self.assertTrue(any(error.startswith(marker) for error in validate(directory)))


if __name__ == "__main__":
    unittest.main()
