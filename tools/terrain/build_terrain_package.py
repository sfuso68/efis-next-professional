#!/usr/bin/env python3
import argparse, hashlib, json, re, shutil
from datetime import datetime, timezone
from pathlib import Path

TILE_RE=re.compile(r"^([NS])(\d{2})([EW])(\d{3})$")
COPERNICUS_TILE_RE=re.compile(r"([NS]\d{2})[_-]?00[_-]?([EW]\d{3})[_-]?00",re.IGNORECASE)
EXPECTED=3601*3601*2

def bounds(key):
    m=TILE_RE.match(key)
    if not m: raise ValueError(f"invalid tile key: {key}")
    lat=int(m.group(2))*(1 if m.group(1)=="N" else -1)
    lon=int(m.group(4))*(1 if m.group(3)=="E" else -1)
    return {"min_lat":lat,"min_lon":lon,"max_lat":lat+1,"max_lon":lon+1}

def tile_key(source: Path):
    direct=source.stem.upper()
    if TILE_RE.match(direct): return direct
    match=COPERNICUS_TILE_RE.search(source.name)
    if match: return (match.group(1)+match.group(2)).upper()
    raise ValueError(f"cannot derive WGS84 tile key from filename: {source.name}")

def convert(source: Path, destination: Path):
    if source.suffix.lower()==".hgt":
        if source.stat().st_size!=EXPECTED: raise ValueError(f"invalid HGT size: {source}")
        shutil.copyfile(source,destination); return "HGT_PASSTHROUGH"
    if source.suffix.lower() not in (".tif",".tiff"): raise ValueError(f"unsupported input: {source}")
    try:
        import numpy as np
        from PIL import Image
    except ImportError as exc: raise RuntimeError("GeoTIFF conversion requires Pillow and NumPy") from exc
    image=Image.open(source)
    if image.size!=(3601,3601): image=image.resize((3601,3601),Image.Resampling.BILINEAR)
    values=np.asarray(image,dtype=np.float64)
    if values.ndim>2: values=values[:,:,0]
    values=np.nan_to_num(values,nan=-32768.0,posinf=32767.0,neginf=-32768.0)
    values=np.clip(np.rint(values),-32768,32767).astype(">i2")
    destination.write_bytes(values.tobytes(order="C")); return "GEOTIFF_TO_HGT_BILINEAR"

def main():
    p=argparse.ArgumentParser(); p.add_argument("--package-id",required=True); p.add_argument("--source-title",required=True); p.add_argument("--source-edition",required=True); p.add_argument("--attribution",required=True); p.add_argument("--vertical-datum",default="EGM2008"); p.add_argument("--status",default="DEVELOPMENT"); p.add_argument("--output",type=Path,required=True); p.add_argument("tiles",type=Path,nargs="+"); a=p.parse_args()
    package=a.output/a.package_id; tile_dir=package/"tiles"; tile_dir.mkdir(parents=True,exist_ok=True)
    records=[]
    for source in a.tiles:
        key=tile_key(source); destination=tile_dir/f"{key}.hgt"; method=convert(source,destination)
        digest=hashlib.sha256(destination.read_bytes()).hexdigest()
        records.append({"key":key,**bounds(key),"rows":3601,"columns":3601,"sample_type":"SIGNED_INT16_BIG_ENDIAN","resolution_arcsec":1,"path":f"tiles/{key}.hgt","bytes":destination.stat().st_size,"sha256":digest,"conversion":method})
    manifest={"schema_version":1,"package_id":a.package_id,"created_utc":datetime.now(timezone.utc).isoformat(),"source":{"title":a.source_title,"edition":a.source_edition,"attribution":a.attribution,"status":a.status},"vertical_datum":a.vertical_datum,"tiles":records}
    (package/"manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps({"package":str(package),"tiles":len(records)},sort_keys=True))
if __name__=="__main__": main()
