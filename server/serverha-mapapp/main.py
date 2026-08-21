import json, os, glob, time
from fastapi import FastAPI, HTTPException, Body
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from app import geo, aero, weather, dist, planner, wx, imagery
MAPDATA="/data"        # /srv/mapdata montato ro
TRIGGERS="/triggers"   # /srv/maptriggers montato rw: solo il nome del job viene scritto
WL={"dem","navdata","osmvector","tiles","hillshade","aeroway","airport-services","circuits","tiles-explode"}
app=FastAPI()
@app.get("/api/status")
def status():
    try: m=json.load(open(f"{MAPDATA}/manifest.json"))
    except Exception: m={}
    return m
@app.get("/api/geojson/{layer}")
def geojson(layer:str):
    fn=geo.LAYERS.get(layer)
    if fn: return fn()
    if layer in aero.AERO_LAYERS: return aero.geojson(layer)
    raise HTTPException(404,"layer")
@app.get("/api/airports")
def api_airports(q:str=""):
    return aero.search(q)
@app.get("/api/airport/{aid}")
def api_airport(aid:int):
    data=aero.airport(aid)
    if not data: raise HTTPException(404,"airport")
    return data
@app.get("/api/metar/{icao}")
def api_metar(icao: str):
    return weather.metar_taf(icao)
@app.post("/api/trigger/{job}", status_code=202)
def trigger(job:str):
    if job not in WL: raise HTTPException(400,"job")
    with open(f"{TRIGGERS}/{job}","w") as f: f.write(str(time.time()))
    return {"accepted":job}
@app.get("/api/logs/{job}")
def logs(job:str):
    if job not in WL: raise HTTPException(400,"job")
    fs=glob.glob(f"{MAPDATA}/logs/{job}-[0-9]*.log")+glob.glob(f"{MAPDATA}/logs/{job}.log")
    if not fs: return {"job":job,"log":""}
    latest=max(fs, key=os.path.getmtime)
    with open(latest) as f: data=f.read()
    return {"job":job,"log":data[-8000:]}
@app.get("/api/pis")
def api_pis():
    return dist.pis()
@app.post("/api/push/{pi}", status_code=202)
def push(pi:str):
    if pi not in dist.pi_names(): raise HTTPException(400,"pi")
    with open(f"{TRIGGERS}/push-{pi}","w") as f: f.write(str(time.time()))
    return {"accepted":pi}
@app.get("/api/push-log/{pi}")
def push_log(pi:str):
    if pi not in dist.pi_names(): raise HTTPException(400,"pi")
    fs=glob.glob(f"{MAPDATA}/logs/push-{pi}-[0-9]*.log")+glob.glob(f"{MAPDATA}/logs/push-{pi}.log")
    if not fs: return {"pi":pi,"log":""}
    latest=max(fs, key=os.path.getmtime)
    with open(latest) as f: data=f.read()
    return {"pi":pi,"log":data[-8000:]}

# --- Pianificatore di volo (v1): calcolo + memorizzazione centrale ---
@app.post("/api/plan")
def api_plan(payload: dict = Body(...)):
    return planner.plan(payload.get("points") or [], payload.get("alt", 3000),
                        payload.get("wind", [0, 0]), payload.get("tas"),
                        payload.get("capacity"), payload.get("load"))
@app.post("/api/plan/save", status_code=201)
def api_plan_save(payload: dict = Body(...)):
    res = planner.save(payload)
    if res.get("error"): raise HTTPException(400, res["error"])
    return res
@app.get("/api/plans")
def api_plans():
    return planner.list_plans()
@app.get("/api/plan/{pid}")
def api_plan_get(pid: str):
    p = planner.load(pid)
    if not p: raise HTTPException(404, "plan")
    return p
@app.delete("/api/plan/{pid}", status_code=204)
def api_plan_del(pid: str):
    planner.delete(pid)
    return None
@app.post("/api/plan/push/{pi}")
def api_plan_push(pi: str, payload: dict = Body(...)):
    if pi not in dist.pi_names(): raise HTTPException(400, "pi")
    res = planner.push_to_pi(pi, payload.get("name"), payload.get("points") or [])
    if res.get("error"): raise HTTPException(502, res["error"])
    return res
@app.post("/api/plan/push-trigger/{pi}", status_code=202)
def api_plan_push_trigger(pi: str, payload: dict = Body(...)):
    # rpi5/sv3d: niente planner HTTP -> scrivo route.txt in un trigger; il
    # dispatcher host lo consegna via SSH (scp + SIGHUP a sv3d).
    if pi not in dist.pi_names(): raise HTTPException(400, "pi")
    txt = planner.route_txt(payload.get("name"), payload.get("points") or [])
    if not txt: raise HTTPException(400, "servono almeno due punti")
    with open(f"{TRIGGERS}/route-{pi}", "w") as f: f.write(txt)
    return {"accepted": pi}

# --- Meteo: radar (RainViewer) + previsione (Open-Meteo) ---
@app.get("/api/radar/frames")
def api_radar_frames():
    return wx.radar_frames()
@app.get("/api/radar/echo")
def api_radar_echo(path: str, lat: float, lon: float, z: int = 8):
    return wx.radar_echo(path, lat, lon, z)
@app.get("/api/wxgrid")
def api_wxgrid(bbox: str, hours: int = 2):
    try:
        a, b, c, d = [float(x) for x in bbox.split(",")]
    except Exception:
        raise HTTPException(400, "bbox=minlon,minlat,maxlon,maxlat")
    return wx.wx_grid(a, b, c, d, max(0, min(hours, 12)))
@app.post("/api/plan/wx")
def api_plan_wx(payload: dict = Body(...)):
    p = planner.plan(payload.get("points") or [], payload.get("alt", 3000),
                     payload.get("wind", [0, 0]), payload.get("tas"), with_terrain=False)
    if p.get("error"):
        raise HTTPException(400, p["error"])
    dep = payload.get("dep") or time.time()
    return wx.wx_route(payload.get("points") or [], p.get("legs") or [], float(dep))

# --- EFIS imagery mission packages: area or flight-plan selection ---
@app.get("/api/imagery/catalog")
def api_imagery_catalog():
    return imagery.catalog()
@app.get("/api/imagery/manifest/{dataset_id}")
def api_imagery_manifest(dataset_id: str):
    path = imagery.manifest_file(dataset_id)
    if not path: raise HTTPException(404, "imagery manifest")
    return FileResponse(path, media_type="application/json")
@app.get("/api/imagery/style/hybrid")
def api_imagery_hybrid_style():
    return imagery.hybrid_style()
@app.api_route("/api/imagery/tile/{dataset_id}/{zoom}/{x}/{y}.webp", methods=["GET","HEAD"])
def api_imagery_tile(dataset_id: str, zoom: int, x: int, y: int):
    path = imagery.tile_file(dataset_id, zoom, x, y)
    if not path: raise HTTPException(404, "imagery tile")
    return FileResponse(path, media_type="image/webp",
                        headers={"Cache-Control":"public,max-age=31536000,immutable"})
@app.post("/api/imagery/plan/area")
def api_imagery_area(payload: dict = Body(...)):
    result = imagery.area_plan(payload)
    if result.get("error"): raise HTTPException(400, result["error"])
    return result
@app.post("/api/imagery/plan/route")
def api_imagery_route(payload: dict = Body(...)):
    result = imagery.route_plan(payload)
    if result.get("error"): raise HTTPException(400, result["error"])
    return result
@app.post("/api/imagery/packages", status_code=201)
def api_imagery_save(payload: dict = Body(...)):
    result = imagery.save(payload)
    if result.get("error"): raise HTTPException(400, result["error"])
    return result
@app.get("/api/imagery/packages")
def api_imagery_packages():
    return imagery.packages()

app.mount("/static", StaticFiles(directory="/app/static"), name="static")
@app.get("/")
def index(): return FileResponse("/app/static/index.html")
