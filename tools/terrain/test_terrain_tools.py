#!/usr/bin/env python3
import importlib.util, io, unittest, zipfile
import numpy as np
from pathlib import Path

ROOT=Path(__file__).resolve().parent
def load(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/f"{name}.py"); module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module); return module

build=load("build_terrain_package")
fetch=load("fetch_copernicus_dem")
prepare=load("prepare_copernicus_stage")
validate=load("validate_terrain_package")

class TerrainToolsTest(unittest.TestCase):
    def test_tile_key_round_trip(self):
        for latitude,longitude in ((45,12),(-3,7),(0,-1)):
            key=validate.encode_key(latitude,longitude)
            self.assertEqual(validate.decode_key(key),(latitude,longitude))

    def test_shared_edge_statistics(self):
        left=np.array([1,2,3,-32768],dtype=np.int16)
        right=np.array([1,3,3,-32768],dtype=np.int16)
        stats=validate.valid_difference(left,right)
        self.assertEqual(stats["samples"],3)
        self.assertEqual(stats["maximum_m"],1)
        self.assertEqual(stats["over_1m"],0)

    def test_copernicus_filename_to_runtime_key(self):
        source=Path("Copernicus_DSM_COG_10_N45_00_E012_00_DEM.tif")
        self.assertEqual(build.tile_key(source),"N45E012")

    def test_bbox_selection_excludes_edge_touch_and_deduplicates(self):
        def product(identifier,date,west,south):
            return {"Id":identifier,"Name":identifier,"S3Path":"/eodata/COP-DEM_GLO-30-DGED/x","PublicationDate":date,"GeoFootprint":{"coordinates":[[[west,south],[west+1,south],[west+1,south+1],[west,south+1],[west,south]]]}}
        payload={"value":[product("old","2024-01-01Z",12,45),product("new","2025-01-01Z",12,45),product("edge","2025-01-01Z",13,45)]}
        selected=fetch.select_products(payload,[12,45,13,46])
        self.assertEqual(len(selected),1); self.assertEqual(selected[0]["Id"],"new"); self.assertEqual(selected[0]["TileKey"],"N45E012")

    def test_zip_path_traversal_is_rejected(self):
        payload=io.BytesIO()
        with zipfile.ZipFile(payload,"w") as archive: archive.writestr("../escape_DEM.tif",b"bad")
        payload.seek(0)
        with zipfile.ZipFile(payload) as archive:
            with self.assertRaises(ValueError): prepare.safe_members(archive)

    def test_download_archive_requires_exactly_one_dem(self):
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            metadata=Path(directory)/"metadata.zip"
            with zipfile.ZipFile(metadata,"w") as archive: archive.writestr("INSPIRE.xml",b"metadata")
            self.assertEqual(fetch.inspect_archive(metadata)["payload_status"],"REJECTED_METADATA_ONLY")
            valid=Path(directory)/"valid.zip"
            with zipfile.ZipFile(valid,"w") as archive: archive.writestr("tile_DEM.tif",b"raster")
            result=fetch.inspect_archive(valid)
            self.assertEqual(result["payload_status"],"DEM_PRESENT"); self.assertEqual(result["dem_count"],1)

if __name__=="__main__": unittest.main()
