#!/usr/bin/env python3
import importlib.util, tempfile, unittest
from pathlib import Path

SPEC=importlib.util.spec_from_file_location("radio",Path(__file__).with_name("import_radionavigation.py")); radio=importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(radio)

class ImportTest(unittest.TestCase):
    def csv(self,text):
        directory=tempfile.TemporaryDirectory(); path=Path(directory.name)/"facilities.csv"; path.write_text(text,encoding="utf-8"); return directory,path
    def test_normalizes_vhf_mhz_and_sorts(self):
        directory,path=self.csv("ident,type,name,frequency,latitude_deg,longitude_deg\nZZZ,VOR,Test,113.20,45.0,12.0\nAAA,NDB,Beacon,365,45.1,12.1\n")
        with directory: result=radio.convert(path)
        self.assertEqual([item["ident"] for item in result],["AAA","ZZZ"]); self.assertEqual(result[1]["frequency_khz"],113200)
    def test_rejects_duplicate(self):
        directory,path=self.csv("ident,type,name,frequency,latitude_deg,longitude_deg\nAAA,VOR,One,113.20,45,12\nAAA,VOR,Two,113.20,45,12\n")
        with directory:
            with self.assertRaisesRegex(ValueError,"duplicate"): radio.convert(path)
    def test_rejects_wrong_band(self):
        directory,path=self.csv("ident,type,name,frequency,latitude_deg,longitude_deg\nAAA,NDB,Wrong,113.20,45,12\n")
        with directory:
            with self.assertRaisesRegex(ValueError,"NDB frequency"): radio.convert(path)

if __name__=="__main__": unittest.main()
