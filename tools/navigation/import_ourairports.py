#!/usr/bin/env python3
import argparse, csv, json
from pathlib import Path

def number(value, default=None):
    try: return float(value) if value not in (None,"") else default
    except ValueError: return default

def main():
    p=argparse.ArgumentParser(); p.add_argument("--airports",type=Path,required=True); p.add_argument("--runways",type=Path,required=True); p.add_argument("--region",default="IT-34"); p.add_argument("--output",type=Path,required=True); a=p.parse_args()
    runway_rows={}
    with a.runways.open(encoding="utf-8",newline="") as f:
        for row in csv.DictReader(f): runway_rows.setdefault(row["airport_ident"],[]).append(row)
    airports=[]
    with a.airports.open(encoding="utf-8",newline="") as f:
        for row in csv.DictReader(f):
            if row["iso_region"] != a.region or row["type"] in ("closed","heliport","balloonport"): continue
            runways=[]
            for r in runway_rows.get(row["ident"],[]):
                low=number(r.get("le_heading_degT")); high=number(r.get("he_heading_degT")); length=number(r.get("length_ft")); width=number(r.get("width_ft"))
                if low is None or high is None or length is None or width is None: continue
                runways.append({"designator_low":r.get("le_ident") or "—","designator_high":r.get("he_ident") or "—",
                    "true_heading_low_deg":low,"true_heading_high_deg":high,"length_m":round(length*.3048,1),"width_m":round(width*.3048,1),
                    "surface":r.get("surface") or "UNKNOWN","status":"DEVELOPMENT"})
            airports.append({"icao":row["ident"],"iata":row.get("iata_code") or None,"name":row["name"],"city":row.get("municipality") or "",
                "country":row["iso_country"],"latitude_deg":number(row["latitude_deg"],0),"longitude_deg":number(row["longitude_deg"],0),
                "elevation_ft":number(row.get("elevation_ft"),0),"airport_type":row["type"].upper(),"status":"DEVELOPMENT","runways":runways})
    result={"database_version":"0.2.0-ourairports-development","source":{"name":"OurAirports Veneto extract","provider":"OurAirports",
        "edition":"runtime-download","source_class":"DEVELOPMENT","licence":"Public Domain; no guarantee of accuracy or fitness"},"airports":airports}
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps({"airports":len(airports),"runways":sum(len(x["runways"]) for x in airports)},sort_keys=True))
if __name__ == "__main__": main()
