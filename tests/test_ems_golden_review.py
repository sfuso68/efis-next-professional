import contextlib
import copy
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import zlib

from tools.ui import prepare_ems_golden_review as review
from tools.ui.capture_ems_responsive_state_matrix import write_manifest
from tools.ui.capture_hmi_golden import png_pixels
from tests.test_hmi_golden_tools import fake_png


class GoldenReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        cls.reference = cls.root / "reference"
        cls.reference.mkdir()
        for w, h, state, _ in review.EXPECTED:
            fake_png(cls.reference / review.filename(w, h, state), w, h)
        cls.manifest = write_manifest(cls.reference, "offscreen")
        cls.files = {p.name: p.read_bytes() for p in cls.reference.iterdir()}

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.addCleanup(self.work.cleanup)
        self.directory = Path(self.work.name)
        self.archive = self.directory / "candidate.zip"

    def archive_from(self, mutation=None, duplicate=False):
        files = dict(self.files)
        if mutation:
            mutation(files)
        with zipfile.ZipFile(self.archive, "w", zipfile.ZIP_DEFLATED) as archive:
            for name, data in files.items():
                archive.writestr(name, data)
            if duplicate:
                with contextlib.redirect_stderr(io.StringIO()):
                    import warnings
                    with warnings.catch_warnings():
                        warnings.simplefilter("ignore", UserWarning)
                        archive.writestr("manifest.json", files["manifest.json"])
        return hashlib.sha256(self.archive.read_bytes()).hexdigest()

    def prepare(self, digest, decisions=None):
        return review.prepare(self.archive, digest, self.reference, "a"*40, 123, 456, decisions)

    def test_complete_candidate_is_only_pending_and_source_unchanged(self):
        digest = self.archive_from()
        report = self.prepare(digest)
        self.assertEqual(report["status"], "PENDING_REVIEW")
        self.assertEqual(len(report["images"]), 18)
        self.assertFalse(report["baseline_update_authorized"])
        self.assertFalse(report["flight_approval"])
        self.assertTrue(all(not item["pixels_changed"] for item in report["images"]))
        self.assertEqual(self.files, {p.name:p.read_bytes() for p in self.reference.iterdir()})

    def test_wrong_external_digest_rejected(self):
        self.archive_from()
        with self.assertRaisesRegex(ValueError, "archive digest differs"):
            self.prepare("f"*64)

    def test_zip_paths_duplicates_and_missing_image_rejected(self):
        for mutation, duplicate in ((lambda f:f.update({"../escape":b"bad"}),False),
                                    (lambda f:f.pop(review.NAMES[0]),False), (None,True)):
            with self.subTest(duplicate=duplicate):
                digest=self.archive_from(mutation,duplicate)
                with self.assertRaisesRegex(ValueError,"archive paths"):
                    self.prepare(digest)
        self.assertFalse((self.directory.parent/"escape").exists())

    def test_wrong_platform_age_order_and_digest_rejected(self):
        for change in (lambda m:m.update(platform="windows"),
                       lambda m:m["images"][0].update(age_ms=1),
                       lambda m:m["images"].reverse(),
                       lambda m:m["images"][0].update(sha256="f"*64),
                       lambda m:m.update(generated_utc="2026-10-22T10:00:00")):
            with self.subTest(change=change):
                def mutation(files):
                    data=copy.deepcopy(self.manifest);change(data)
                    files["manifest.json"]=json.dumps(data).encode()
                digest=self.archive_from(mutation)
                with self.assertRaises(ValueError):
                    self.prepare(digest)

    def test_review_is_bound_to_each_pixel_and_full_commit(self):
        rows=self.manifest["images"]
        digest="a"*64
        decisions=review.read_review(None,digest,"b"*40,rows,"d"*64)
        body={"schema":"avionix.ems-golden-review-decisions.v1","archive_sha256":digest,
              "source_commit":"b"*40,"reference_manifest_sha256":"d"*64,"decisions":decisions}
        path=self.directory/"review.json"
        for mutation in (lambda x:x.update(source_commit="c"*40),
                         lambda x:x.update(reference_manifest_sha256="e"*64),
                         lambda x:x["decisions"][0].update(pixel_sha256="f"*64),
                         lambda x:x["decisions"].pop(),
                         lambda x:x["decisions"][0].update(decision="ACCEPT")):
            data=copy.deepcopy(body);mutation(data);path.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                review.read_review(path,digest,"b"*40,rows,"d"*64)

    def test_even_all_accepted_reviews_do_not_authorize_baseline(self):
        digest=self.archive_from()
        reference_digest=hashlib.sha256((self.reference/"manifest.json").read_bytes()).hexdigest()
        decisions=review.read_review(None,digest,"a"*40,self.manifest["images"],reference_digest)
        for row in decisions:
            row.update(decision="ACCEPT",reviewer="SYNTHETIC TEST ONLY",reason="synthetic fixture")
        path=self.directory/"review.json"
        path.write_text(json.dumps({"schema":"avionix.ems-golden-review-decisions.v1",
            "archive_sha256":digest,"source_commit":"a"*40,"reference_manifest_sha256":reference_digest,"decisions":decisions}))
        report=self.prepare(digest,path)
        self.assertEqual(report["status"],"REVIEW_COMPLETE_NEEDS_AUTHORIZATION")
        self.assertFalse(report["baseline_update_authorized"])

    def test_cli_never_overwrites_or_writes_inside_reference(self):
        output=self.directory/"existing.json";output.write_text("PRESERVE")
        for target in (output,self.reference/"new.json"):
            args=["review","--archive",str(self.archive),"--sha256","a"*64,
                "--reference",str(self.reference),"--source-commit","a"*40,
                "--run-id","1","--artifact-id","2","--output",str(target)]
            with patch.object(sys,"argv",args),contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(review.main(),1)
        self.assertEqual(output.read_text(),"PRESERVE")
        self.assertFalse((self.reference/"new.json").exists())

    def test_png_crc_size_and_inflate_bombs_fail_closed(self):
        def chunk(kind,payload):
            return struct.pack(">I",len(payload))+kind+payload+struct.pack(">I",zlib.crc32(kind+payload))
        path=self.directory/"bad.png"
        normal=self.files[review.NAMES[0]]
        crc_bad=bytearray(normal);crc_bad[29]^=1
        header=chunk(b"IHDR",struct.pack(">IIBBBBB",1,1,8,6,0,0,0))
        bomb=b"\x89PNG\r\n\x1a\n"+header+chunk(b"IDAT",zlib.compress(b"\0"*1000000))+chunk(b"IEND",b"")
        huge=b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",100000,100000,8,6,0,0,0))+chunk(b"IEND",b"")
        animated=normal[:33]+chunk(b"acTL",struct.pack(">II",2,0))+normal[33:]
        for data in (bytes(crc_bad),normal[:-2],bomb,huge,normal+b"extra",animated):
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                png_pixels(path)

    def test_symlink_and_oversized_zip_entries_rejected(self):
        for symlink in (True,False):
            with self.subTest(symlink=symlink):
                with zipfile.ZipFile(self.archive,"w",zipfile.ZIP_DEFLATED) as archive:
                    for name,data in self.files.items():
                        if name==review.NAMES[0]:
                            if symlink:
                                entry=zipfile.ZipInfo(name);entry.external_attr=(0o120777<<16)
                                archive.writestr(entry,b"/not-a-real-target")
                            else:
                                archive.writestr(name,b"x"*8388609)
                        else:
                            archive.writestr(name,data)
                digest=hashlib.sha256(self.archive.read_bytes()).hexdigest()
                with self.assertRaisesRegex(ValueError,"archive entry"):
                    self.prepare(digest)

    def test_duplicate_json_fields_rejected(self):
        with self.assertRaisesRegex(ValueError,"duplicate JSON field"):
            review.strict_json('{"status":"ACCEPT","status":"NOT_REVIEWED"}')


if __name__ == "__main__":
    unittest.main()
