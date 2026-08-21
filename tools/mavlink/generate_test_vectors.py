#!/usr/bin/env python3
"""Genera i vettori di prova del parser MAVLink di EFIS NEXT.

Un parser verificato contro frame costruiti da chi lo ha scritto conferma
soltanto che l'autore e' coerente con se stesso. Questi vettori sono invece
prodotti da pymavlink, l'implementazione di riferimento: se il parser li
interpreta correttamente, lo fa perche' rispetta il protocollo e non perche'
condivide un malinteso con i propri test.

I frame coprono MAVLink 2 e MAVLink 1, il troncamento degli zeri finali, il
payload firmato e i valori negativi. Il file prodotto viene versionato: la
generazione non fa parte della build.

Uso:

    python -m venv .venv && .venv/bin/pip install pymavlink
    .venv/bin/python tools/mavlink/generate_test_vectors.py \\
        --output tests/mavlink_reference_frames.hpp
"""

from __future__ import annotations

import argparse
import warnings
from pathlib import Path

warnings.filterwarnings("ignore", message=".*deprecated.*")


class Collector:
    """Raccoglie i byte prodotti da pymavlink invece di scriverli su una porta."""

    def __init__(self) -> None:
        self.buffer = bytearray()

    def write(self, data) -> None:
        self.buffer.extend(data)


def build(version2: bool, signing: bool = False, dialect: str = "ardupilotmega"):
    import importlib

    v20 = importlib.import_module("pymavlink.dialects.v20." + dialect)
    from pymavlink.dialects.v10 import ardupilotmega as v10

    collector = Collector()
    module = v20 if version2 else v10
    link = module.MAVLink(collector, srcSystem=1, srcComponent=1)
    if not version2:
        link.WIRE_PROTOCOL_VERSION = "1.0"
    if signing:
        link.signing.secret_key = bytes(range(32))
        link.signing.sign_outgoing = True
        link.signing.link_id = 0
        link.signing.timestamp = 1
    return link, collector


def case(name, description, checks, version2=True, signing=False, emit=None,
         dialect="ardupilotmega"):
    link, collector = build(version2, signing, dialect)
    emit(link)
    return {
        "name": name,
        "description": description,
        "bytes": bytes(collector.buffer),
        "checks": checks,
        "version2": version2,
        "signed": signing,
    }


def collect():
    from pymavlink.dialects.v20 import ardupilotmega as v20

    cases = []

    # Assetto: valori in virgola mobile, positivi e negativi.
    cases.append(case(
        "attitude",
        "ATTITUDE con rollio negativo: verifica la virgola mobile e il segno",
        [("ATTITUDE", "roll", -0.5235988), ("ATTITUDE", "pitch", 0.1745329),
         ("ATTITUDE", "yaw", 1.5707963), ("ATTITUDE", "time_boot_ms", 123456)],
        emit=lambda m: m.attitude_send(123456, -0.5235988, 0.1745329, 1.5707963,
                                       0.01, -0.02, 0.03)))

    # Dati aria: e' il percorso del sensore Matek. VFR_HUD porta la velocita'
    # calcolata, SCALED_PRESSURE la pressione differenziale grezza.
    cases.append(case(
        "vfr_hud",
        "VFR_HUD: velocita' indicata, al suolo, quota e velocita' verticale",
        [("VFR_HUD", "airspeed", 32.5), ("VFR_HUD", "groundspeed", 30.25),
         ("VFR_HUD", "alt", 1524.0), ("VFR_HUD", "climb", -2.5),
         ("VFR_HUD", "heading", 42), ("VFR_HUD", "throttle", 65)],
        emit=lambda m: m.vfr_hud_send(32.5, 30.25, 42, 65, 1524.0, -2.5)))

    cases.append(case(
        "scaled_pressure",
        "SCALED_PRESSURE: pressione differenziale del sensore aria e temperatura",
        [("SCALED_PRESSURE", "press_abs", 1013.25),
         ("SCALED_PRESSURE", "press_diff", 0.1718),
         ("SCALED_PRESSURE", "temperature", 2350)],
        emit=lambda m: m.scaled_pressure_send(555000, 1013.25, 0.1718, 2350)))

    # Satellitare senza fix: il caso che il paragrafo 13 dell'handoff impone di
    # trattare come invalido invece che come posizione approssimata.
    cases.append(case(
        "gps_no_fix",
        "GPS_RAW_INT senza fix: coordinate presenti ma prive di significato",
        [("GPS_RAW_INT", "fix_type", 1), ("GPS_RAW_INT", "satellites_visible", 0),
         ("GPS_RAW_INT", "lat", 456484000), ("GPS_RAW_INT", "eph", 65535)],
        emit=lambda m: m.gps_raw_int_send(555000, 1, 456484000, 121944000,
                                          59000, 65535, 65535, 0, 0, 0)))

    cases.append(case(
        "gps_fix_3d",
        "GPS_RAW_INT con fix tridimensionale e satelliti sufficienti",
        [("GPS_RAW_INT", "fix_type", 3), ("GPS_RAW_INT", "satellites_visible", 14),
         ("GPS_RAW_INT", "lat", 456484000), ("GPS_RAW_INT", "lon", 121944000),
         ("GPS_RAW_INT", "alt", 59000), ("GPS_RAW_INT", "eph", 90)],
        emit=lambda m: m.gps_raw_int_send(555000, 3, 456484000, 121944000,
                                          59000, 90, 130, 1250, 4200, 14)))

    # Salute dei sottosistemi: maschere di bit a 32 bit, dove perdere precisione
    # significherebbe perdere lo stato di un sensore.
    cases.append(case(
        "sys_status",
        "SYS_STATUS: maschere di presenza, abilitazione e salute dei sensori",
        [("SYS_STATUS", "onboard_control_sensors_present", 0x1400002F),
         ("SYS_STATUS", "onboard_control_sensors_health", 0x1400000F),
         ("SYS_STATUS", "voltage_battery", 12450),
         ("SYS_STATUS", "current_battery", 1830)],
        emit=lambda m: m.sys_status_send(0x1400002F, 0x1400002F, 0x1400000F,
                                         420, 12450, 1830, 78, 0, 0, 0, 0, 0, 0)))

    # Tempo UTC dichiarato dall'autopilota.
    cases.append(case(
        "system_time",
        "SYSTEM_TIME: riferimento UTC, quando il nodo ne dispone",
        [("SYSTEM_TIME", "time_unix_usec", 1755739800000000),
         ("SYSTEM_TIME", "time_boot_ms", 555000)],
        emit=lambda m: m.system_time_send(1755739800000000, 555000)))

    # Battito: identifica il nodo e il suo stato.
    cases.append(case(
        "heartbeat",
        "HEARTBEAT di ArduPlane in volo",
        [("HEARTBEAT", "type", 1), ("HEARTBEAT", "autopilot", 3),
         ("HEARTBEAT", "custom_mode", 10), ("HEARTBEAT", "system_status", 4)],
        emit=lambda m: m.heartbeat_send(1, 3, 217, 10, 4)))

    # Testo: array di caratteri non necessariamente terminato.
    cases.append(case(
        "statustext",
        "STATUSTEXT: testo in un campo di caratteri",
        [("STATUSTEXT", "severity", 4)],
        emit=lambda m: m.statustext_send(4, b"PreArm: GPS bad fix")))

    # Stesso messaggio in MAVLink 1: intestazione diversa, stesso contenuto.
    cases.append(case(
        "attitude_v1",
        "ATTITUDE in MAVLink 1: intestazione a sei byte, nessun campo di estensione",
        [("ATTITUDE", "roll", -0.5235988), ("ATTITUDE", "yaw", 1.5707963)],
        version2=False,
        emit=lambda m: m.attitude_send(123456, -0.5235988, 0.1745329, 1.5707963,
                                       0.01, -0.02, 0.03)))

    # Frame firmato: tredici byte in coda che vanno scavalcati senza confonderli
    # con l'inizio del messaggio successivo.
    cases.append(case(
        "heartbeat_signed",
        "HEARTBEAT firmato: la firma va consumata, non interpretata",
        [("HEARTBEAT", "type", 1), ("HEARTBEAT", "custom_mode", 10)],
        signing=True,
        emit=lambda m: m.heartbeat_send(1, 3, 217, 10, 4)))

    # Zeri finali rimossi: POWER_STATUS con tutti i campi nulli si riduce a un
    # payload vuoto, e i suoi campi valgono comunque zero.
    cases.append(case(
        "power_status_trimmed",
        "POWER_STATUS interamente nullo: MAVLink 2 elimina gli zeri finali",
        [("POWER_STATUS", "Vcc", 0), ("POWER_STATUS", "Vservo", 0)],
        emit=lambda m: m.power_status_send(0, 0, 0)))

    # AIRSPEED, il messaggio dedicato al sensore aria. Porta la pressione
    # differenziale grezza, la temperatura dell'elemento e i flag di stato: e'
    # il contenuto che permette di sfruttare il Matek ASPD-DLVR oltre alla sola
    # velocita' calcolata. Nel dialetto ardupilotmega di questa versione di
    # pymavlink non c'e' ancora, quindi il frame viene prodotto dal dialetto
    # `all`, che include le definizioni in sviluppo.
    #
    # Il valore della pressione grezza corrisponde all'offset differenziale
    # osservato al banco e riportato al paragrafo 14 dell'handoff.
    cases.append(case(
        "airspeed",
        "AIRSPEED: pressione differenziale grezza, temperatura e flag del sensore",
        [("AIRSPEED", "id", 0), ("AIRSPEED", "airspeed", 8.33),
         ("AIRSPEED", "temperature", 2415), ("AIRSPEED", "raw_press", 17.18),
         ("AIRSPEED", "flags", 0)],
        dialect="all",
        emit=lambda m: m.airspeed_send(0, 8.33, 2415, 17.18, 0)))

    return cases


def render(cases, version: str) -> str:
    out = []
    w = out.append
    w("#pragma once")
    w("")
    w("// FILE GENERATO. Non modificare a mano.")
    w("//")
    w("// Prodotto da tools/mavlink/generate_test_vectors.py a partire da")
    w(f"// pymavlink {version}, dialetto ardupilotmega.")
    w("//")
    w("// Sono frame veri, codificati dall'implementazione di riferimento. Servono a")
    w("// verificare che il parser di EFIS NEXT rispetti il protocollo, e non")
    w("// soltanto che sia coerente con le aspettative di chi lo ha scritto.")
    w("")
    w("#include <cstdint>")
    w("")
    w("namespace efis::mavlink::reference {")
    w("")

    for entry in cases:
        w(f"// {entry['description']}")
        w(f"inline constexpr std::uint8_t {entry['name']}[] = {{")
        data = entry["bytes"]
        for start in range(0, len(data), 12):
            chunk = data[start:start + 12]
            w("    " + " ".join(f"0x{b:02X}," for b in chunk))
        w("};")
        w("")

    w("}  // namespace efis::mavlink::reference")
    w("")
    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    import pymavlink

    cases = collect()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(cases, pymavlink.__version__),
                           encoding="utf-8", newline="\n")

    print(f"{len(cases)} vettori scritti in {args.output}")
    for entry in cases:
        kind = "v2" if entry["version2"] else "v1"
        if entry["signed"]:
            kind += " firmato"
        print(f"  {entry['name']:<24} {len(entry['bytes']):>3} byte  {kind}")
        for message, field, value in entry["checks"]:
            print(f"      atteso {message}.{field} = {value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
