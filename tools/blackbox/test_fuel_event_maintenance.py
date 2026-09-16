#!/usr/bin/env python3
import json
import struct
import tempfile
import unittest
from pathlib import Path

import fuel_event_maintenance as maintenance


def write_segment(path, records=2, trailing=b""):
    header=struct.pack("<8sHHQB3x40s",b"FUSOBBX1",1,24,123456789,1,b"can_a\0")
    record=struct.pack("<QIBBH8s",123456790,0x123,2,0,0,b"\x01\x02"+b"\0"*6)
    path.write_bytes(header+record*records+trailing)


class CanManifestTests(unittest.TestCase):
    def test_seals_only_closed_segments_and_reuses_unchanged_digest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);can=root/"can";state=root/"state";can.mkdir();key=b"test-key"
            first=can/"bbx-20260831T100000Z.bbx";write_segment(first)
            write_segment(can/"bbx-20260831T100500Z.bbx.part")
            result=maintenance.seal_can(state,can,key)
            self.assertEqual(result["summary"]["segments"],1)
            self.assertEqual(result["summary"]["valid"],1)
            self.assertTrue(maintenance.verify(state/"can-segment-manifest.json",key))
            original=result["segments"][0]["sha256"]
            repeated=maintenance.seal_can(state,can,key)
            self.assertEqual(repeated["segments"][0]["sha256"],original)

    def test_reports_trailing_record_without_discarding_recoverable_segment(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);can=root/"can";state=root/"state";can.mkdir();key=b"test-key"
            write_segment(can/"bbx-20260831T101000Z.bbx",trailing=b"partial")
            result=maintenance.seal_can(state,can,key)
            item=result["segments"][0]
            self.assertEqual(item["format"],"RECOVERED_TRUNCATED")
            self.assertEqual(item["records"],2)
            self.assertEqual(item["trailing_bytes"],7)
            self.assertEqual(result["summary"]["valid"],1)

    def test_rejects_wrong_magic_and_removes_pruned_segments(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);can=root/"can";state=root/"state";can.mkdir();key=b"test-key"
            good=can/"bbx-20260831T102000Z.bbx";write_segment(good)
            bad=can/"bbx-20260831T102500Z.bbx";bad.write_bytes(b"NOTABBOX"+b"\0"*56)
            first=maintenance.seal_can(state,can,key)
            self.assertEqual(first["summary"]["rejected"],1)
            good.unlink()
            second=maintenance.seal_can(state,can,key)
            self.assertEqual([item["name"] for item in second["segments"]],[bad.name])


if __name__=="__main__": unittest.main()
