#!/usr/bin/env python3
"""Convert a reviewed radionavigation CSV export into an EFIS NEXT index."""
import argparse, csv, hashlib, json, re, sys
from pathlib import Path

TYPES={"VOR","VOR_DME","VORTAC","NDB","DME","ILS","LOC"}
IDENT=re.compile(r"^[A-Z0-9]{1,5}$")

def number(row,name,required=True):
    value=(row.get(name) or "").strip()
    if not value and not required: return None
    try: return float(value)
    except ValueError as exc: raise ValueError(f"row {row['_row']}: invalid {name}") from exc

def frequency_khz(row,facility_type):
    value=number(row,"frequency")
    khz=round(value*1000 if value<1000 and facility_type!="NDB" else value)
    if facility_type=="NDB" and not 190<=khz<=1750: raise ValueError(f"row {row['_row']}: NDB frequency outside 190..1750 kHz")
    if facility_type in {"VOR","VOR_DME","VORTAC","DME"} and not 108000<=khz<=117950: raise ValueError(f"row {row['_row']}: VHF navigation frequency outside 108.000..117.950 MHz")
    if facility_type in {"ILS","LOC"} and not 108100<=khz<=111950: raise ValueError(f"row {row['_row']}: ILS/LOC frequency outside 108.100..111.950 MHz")
    return khz

def convert(source):
    facilities=[]; keys=set()
    with source.open("r",encoding="utf-8-sig",newline="") as stream:
        reader=csv.DictReader(stream)
        required={"ident","type","name","frequency","latitude_deg","longitude_deg"}
        if not reader.fieldnames or not required.issubset(reader.fieldnames): raise ValueError("CSV missing required columns: "+", ".join(sorted(required)))
        for row_number,row in enumerate(reader,2):
            row["_row"]=row_number; ident=(row.get("ident") or "").strip().upper(); facility_type=(row.get("type") or "").strip().upper()
            if not IDENT.fullmatch(ident): raise ValueError(f"row {row_number}: invalid ident")
            if facility_type not in TYPES: raise ValueError(f"row {row_number}: unsupported type {facility_type}")
            lat=number(row,"latitude_deg"); lon=number(row,"longitude_deg")
            if not -90<=lat<=90 or not -180<=lon<=180: raise ValueError(f"row {row_number}: coordinates outside WGS84 bounds")
            frequency=frequency_khz(row,facility_type); key=(ident,facility_type,frequency)
            if key in keys: raise ValueError(f"row {row_number}: duplicate facility {ident}/{facility_type}/{frequency}")
            keys.add(key)
            facilities.append({"ident":ident,"type":facility_type,"name":(row.get("name") or "").strip(),"frequency_khz":frequency,"latitude_deg":lat,"longitude_deg":lon,"elevation_ft":number(row,"elevation_ft",False),"magnetic_variation_deg":number(row,"magnetic_variation_deg",False),"service_volume":(row.get("service_volume") or "").strip() or None,"status":"DEVELOPMENT","source_record_id":(row.get("source_record_id") or "").strip() or None})
    return sorted(facilities,key=lambda item:(item["ident"],item["type"],item["frequency_khz"]))

def main():
    parser=argparse.ArgumentParser(); parser.add_argument("source",type=Path); parser.add_argument("--output",type=Path,required=True); parser.add_argument("--database-version",required=True); parser.add_argument("--source-name",required=True); parser.add_argument("--source-edition",required=True); parser.add_argument("--attribution",required=True); parser.add_argument("--reference"); parser.add_argument("--effective-from"); parser.add_argument("--effective-to"); args=parser.parse_args()
    digest=hashlib.sha256(args.source.read_bytes()).hexdigest(); facilities=convert(args.source)
    document={"schema_version":1,"database_version":args.database_version,"source":{"name":args.source_name,"edition":args.source_edition,"effective_from":args.effective_from,"effective_to":args.effective_to,"status":"DEVELOPMENT","attribution":args.attribution,"sha256":digest,"reference":args.reference},"facilities":facilities}
    args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_text(json.dumps(document,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps({"output":str(args.output),"facilities":len(facilities),"sha256":digest,"status":"DEVELOPMENT"},sort_keys=True))

if __name__=="__main__":
    try: main()
    except (ValueError,OSError) as exc: print(f"error: {exc}",file=sys.stderr); raise SystemExit(2)
