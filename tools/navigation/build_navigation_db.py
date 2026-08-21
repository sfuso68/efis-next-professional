#!/usr/bin/env python3
import argparse, hashlib, json, sqlite3
from pathlib import Path

def build(schema: Path, seed: Path, output: Path) -> dict:
    data=json.loads(seed.read_text(encoding="utf-8"))
    output.parent.mkdir(parents=True,exist_ok=True)
    if output.exists(): output.unlink()
    db=sqlite3.connect(output)
    try:
        db.executescript(schema.read_text(encoding="utf-8"))
        db.execute("INSERT INTO nav_meta VALUES('schema_version','1')")
        db.execute("INSERT INTO nav_meta VALUES('database_version',?)",(data["database_version"],))
        source=data["source"]
        cur=db.execute("INSERT INTO data_sources(name,provider,edition,licence,source_class) VALUES(?,?,?,?,?)",
            (source["name"],source["provider"],source.get("edition"),source.get("licence"),source["source_class"]))
        source_id=cur.lastrowid
        runway_count=0
        for airport in data["airports"]:
            cur=db.execute("INSERT INTO airports(icao,iata,name,city,country,latitude_deg,longitude_deg,elevation_ft,airport_type,source_id,status) VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                tuple(airport.get(k) for k in ("icao","iata","name","city","country","latitude_deg","longitude_deg","elevation_ft","airport_type"))+(source_id,airport["status"]))
            for runway in airport.get("runways",[]):
                db.execute("INSERT INTO runways(airport_id,designator_low,designator_high,true_heading_low_deg,true_heading_high_deg,length_m,width_m,surface,source_id,status) VALUES(?,?,?,?,?,?,?,?,?,?)",
                    (cur.lastrowid,runway["designator_low"],runway["designator_high"],runway["true_heading_low_deg"],runway["true_heading_high_deg"],runway["length_m"],runway["width_m"],runway.get("surface"),source_id,runway["status"]))
                runway_count+=1
        db.commit()
        assert db.execute("PRAGMA integrity_check").fetchone()[0] == "ok"
        return {"airports":len(data["airports"]),"runways":runway_count}
    finally: db.close()

def write_index(database: Path) -> Path:
    db=sqlite3.connect(database); db.row_factory=sqlite3.Row
    try:
        version=db.execute("SELECT value FROM nav_meta WHERE key='database_version'").fetchone()[0]
        airports=[]
        for row in db.execute("SELECT a.*,s.name source_name,s.source_class FROM airports a JOIN data_sources s ON s.id=a.source_id ORDER BY a.icao"):
            item=dict(row); item["runways"]=[dict(r) for r in db.execute("SELECT designator_low,designator_high,true_heading_low_deg,true_heading_high_deg,length_m,width_m,surface,status FROM runways WHERE airport_id=?",(item["id"],))]
            airports.append(item)
    finally: db.close()
    path=database.with_name("navigation-index.json")
    payload={"schema_version":1,"database_version":version,"airports":airports}
    path.write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding="utf-8")
    return path

if __name__ == "__main__":
    p=argparse.ArgumentParser(); p.add_argument("--schema",type=Path,required=True); p.add_argument("--seed",type=Path,required=True); p.add_argument("--output",type=Path,required=True); a=p.parse_args()
    print(json.dumps(build(a.schema,a.seed,a.output),sort_keys=True)); print(write_index(a.output))
