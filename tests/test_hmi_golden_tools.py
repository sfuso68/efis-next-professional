import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from tools.ui.capture_hmi_golden import png_pixel_sha256, png_size
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


class HmiGoldenToolsTests(unittest.TestCase):
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
            images = []
            for width, height in ((800, 480), (1024, 600), (1280, 800)):
                path = directory / f"ems-{width}x{height}.png"
                fake_png(path, width, height)
                images.append({"file": path.name, "width": width, "height": height,
                               "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                               "pixel_sha256": png_pixel_sha256(path)})
            manifest = {"schema": "avionix.hmi-golden-manifest.v1",
                        "status": "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE",
                        "images": images}
            (directory / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(validate(directory), [])
            (directory / "ems-800x480.png").write_bytes(b"tampered")
            self.assertTrue(validate(directory))

    def test_lost_manifest_records_state_and_age(self):
        manifest = {"data_state": "LOST", "data_age_ms": 1450}
        self.assertEqual(manifest["data_state"], "LOST")
        self.assertGreater(manifest["data_age_ms"], 1000)


if __name__ == "__main__":
    unittest.main()
