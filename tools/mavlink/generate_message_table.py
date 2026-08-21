#!/usr/bin/env python3
"""Genera la tabella dei messaggi MAVLink usata dal parser di EFIS NEXT.

Il core di EFIS NEXT non dipende da alcuna libreria MAVLink: il parser e' scritto
nel progetto. Restano pero' due informazioni che non possono essere inventate,
perche' sono definite dal protocollo e un errore di trascrizione le renderebbe
sbagliate in modo silenzioso:

  - il CRC_EXTRA di ogni messaggio, che entra nel calcolo del checksum. Un valore
    sbagliato non produce un errore visibile: fa semplicemente scartare tutti i
    frame di quel tipo, come se il sensore non trasmettesse;
  - l'ordine dei campi sul filo, che in MAVLink non e' quello di dichiarazione ma
    quello per dimensione decrescente.

Questo strumento estrae entrambe dall'implementazione di riferimento pymavlink e
le scrive in un'intestazione C++ leggibile e verificabile. Il risultato viene
versionato: la generazione non fa parte della build e non introduce alcuna
dipendenza a runtime.

Uso:

    python -m venv .venv && .venv/bin/pip install pymavlink
    .venv/bin/python tools/mavlink/generate_message_table.py \\
        --output include/efis/providers/mavlink_message_table.hpp

Il file generato non va modificato a mano: si rigenera.
"""

from __future__ import annotations

import argparse
import warnings
from pathlib import Path

warnings.filterwarnings("ignore", message=".*deprecated.*")

# Messaggi che EFIS NEXT interpreta. La selezione copre l'intero contenuto
# sensoristico che ArduPlane espone su MAVLink: assetto, posizione, dati aria,
# unita' inerziali, alimentazione, salute dei sottosistemi e navigazione.
#
# Il sensore aria Matek ASPD-DLVR, collegato in DroneCAN, non ha un messaggio
# proprio: arriva attraverso SCALED_PRESSURE (pressione differenziale grezza e
# temperatura), VFR_HUD (velocita' calcolata), AIRSPEED_AUTOCAL (stato della
# calibrazione e rapporto) e il bit di salute in SYS_STATUS. Sono qui tutti e
# quattro, perche' il valore utile del sensore non e' solo la velocita'.
MESSAGES = [
    # Sistema, tempo e salute
    "HEARTBEAT",
    "SYS_STATUS",
    "SYSTEM_TIME",
    "POWER_STATUS",
    "BATTERY_STATUS",
    "MEMINFO",
    "HWSTATUS",
    "STATUSTEXT",
    "EKF_STATUS_REPORT",
    "VIBRATION",
    # Assetto
    "ATTITUDE",
    "ATTITUDE_QUATERNION",
    "AHRS",
    "AHRS2",
    # Posizione e satellitare
    "GPS_RAW_INT",
    "GPS2_RAW",
    "GLOBAL_POSITION_INT",
    "HOME_POSITION",
    "TERRAIN_REPORT",
    # Dati aria
    "VFR_HUD",
    "SCALED_PRESSURE",
    "SCALED_PRESSURE2",
    "SCALED_PRESSURE3",
    "AIRSPEED_AUTOCAL",
    "WIND",
    # Unita' inerziali
    "RAW_IMU",
    "SCALED_IMU2",
    "SCALED_IMU3",
    "SENSOR_OFFSETS",
    # Navigazione
    "NAV_CONTROLLER_OUTPUT",
    "MISSION_CURRENT",
    # Comandi e superfici
    "RC_CHANNELS",
    "SERVO_OUTPUT_RAW",
    # Angolo d'attacco e derapata, e stato del microcontrollore. Entrambi
    # osservati nel flusso reale di ArduPlane 4.7.0.
    "AOA_SSA",
    "MCU_STATUS",
]

# Messaggi definiti qui perche' la versione di pymavlink usata come riferimento
# non li conosce ancora, pur essendo trasmessi dal firmware installato.
#
# AIRSPEED (295) e' il caso che conta: e' il messaggio dedicato al sensore aria,
# e nel flusso reale di ArduPlane 4.7.0 compare come UNKNOWN_295. Trasporta la
# pressione differenziale grezza, la temperatura dell'elemento e i flag di stato,
# cioe' esattamente cio' che serve per sfruttare il Matek ASPD-DLVR oltre alla
# sola velocita' calcolata.
#
# Definirlo a mano sarebbe un rischio se restasse non verificato. Non lo e': il
# CRC_EXTRA viene calcolato da questa definizione con lo stesso procedimento del
# protocollo, e una sola parola sbagliata fra nomi e tipi produrrebbe un valore
# diverso, con cui nessun frame reale supererebbe il controllo. La verifica
# contro la registrazione vera e' in tests/data/mavlink.
#
# Ordine di dichiarazione: il generatore lo riordina per dimensione decrescente
# come fa il protocollo.
LOCAL_MESSAGES = [
    {
        "id": 295,
        "name": "AIRSPEED",
        "fields": [
            ("uint8_t", "id", 0),
            ("float", "airspeed", 0),
            ("int16_t", "temperature", 0),
            ("float", "raw_press", 0),
            ("uint8_t", "flags", 0),
        ],
    },
]

TYPE_SIZES = {
    "uint8_t": 1, "int8_t": 1, "char": 1,
    "uint16_t": 2, "int16_t": 2,
    "uint32_t": 4, "int32_t": 4, "float": 4,
    "uint64_t": 8, "int64_t": 8, "double": 8,
    "uint8_t_mavlink_version": 1,
}

TYPE_ENUM = {
    "uint8_t": "u8", "uint8_t_mavlink_version": "u8", "int8_t": "i8", "char": "ch",
    "uint16_t": "u16", "int16_t": "i16",
    "uint32_t": "u32", "int32_t": "i32", "float": "f32",
    "uint64_t": "u64", "int64_t": "i64", "double": "f64",
}


def crc_extra_over(message_name, fields):
    """Ricalcola il CRC_EXTRA come prescritto dal protocollo.

    Serve a due scopi. Verifica che l'elenco dei campi estratto qui sia
    esattamente quello su cui il protocollo calcola il checksum: se un tipo o un
    ordine fossero sbagliati nessun risultato coinciderebbe con quello di
    riferimento, e la generazione fallirebbe invece di produrre una tabella
    plausibile ma inservibile. E, provandolo sui prefissi dell'elenco, individua
    dove finiscono i campi base e cominciano le estensioni, perche' il CRC_EXTRA
    e' definito sui soli campi base.
    """
    from pymavlink.generator.mavcrc import x25crc

    crc = x25crc()
    crc.accumulate_str(message_name + " ")
    for field in fields:
        crc.accumulate_str(field["wire_type"] + " ")
        crc.accumulate_str(field["name"] + " ")
        if field["array_length"]:
            crc.accumulate([field["array_length"]])
    return (crc.crc & 0xFF) ^ (crc.crc >> 8)


def describe(message_class):
    """Restituisce id, crc_extra e i campi in ordine di filo con i loro offset."""
    declared = list(message_class.fieldnames)
    types = list(message_class.fieldtypes)
    arrays = list(message_class.array_lengths)

    fields = []
    offset = 0
    # Attenzione ai due indici diversi: in pymavlink `fieldtypes` segue l'ordine
    # di dichiarazione, mentre `array_lengths` segue l'ordine di filo. Usare lo
    # stesso indice per entrambi attribuisce la lunghezza di un array al campo
    # sbagliato, e sposta tutti gli offset successivi. Il controllo sul CRC_EXTRA
    # piu' avanti esiste anche per intercettare errori come questo.
    for position, name in enumerate(message_class.ordered_fieldnames):
        field_type = types[declared.index(name)]
        count = arrays[position] or 0
        size = TYPE_SIZES[field_type] * (count if count else 1)
        fields.append({
            "name": name,
            "type": TYPE_ENUM[field_type],
            # Nome del tipo come lo vede il calcolo del CRC_EXTRA: la variante
            # uint8_t_mavlink_version vi compare come uint8_t.
            "wire_type": "uint8_t" if field_type == "uint8_t_mavlink_version" else field_type,
            "array_length": count,
            "offset": offset,
            "size": size,
        })
        offset += size

    # I campi base sono il prefisso il cui CRC_EXTRA coincide con quello di
    # riferimento. Cio' che segue e' estensione: un trasmettitore piu' vecchio
    # puo' non inviarlo affatto, e in quel caso il valore non e' zero, e' assente.
    name = message_class.msgname
    expected = message_class.crc_extra
    base_count = None
    for count in range(len(fields), -1, -1):
        if crc_extra_over(name, fields[:count]) == expected:
            base_count = count
            break
    if base_count is None:
        raise SystemExit(
            f"{name}: nessun prefisso dei campi riproduce il CRC_EXTRA {expected}. "
            "La descrizione dei campi non e' affidabile e la tabella non viene scritta.")

    if base_count:
        last = fields[base_count - 1]
        base_length = last["offset"] + last["size"]
    else:
        base_length = 0

    return {
        "id": message_class.id,
        "name": name,
        "crc_extra": expected,
        "length": offset,
        "base_length": base_length,
        "base_count": base_count,
        "fields": fields,
    }


def describe_local(definition):
    """Descrive un messaggio definito nel progetto anziche' nel dialetto.

    Segue le stesse regole del protocollo: riordino per dimensione decrescente e
    CRC_EXTRA calcolato sull'elenco completo dei campi. Non avendo estensioni,
    base_length coincide con la lunghezza del payload.
    """
    ordered = sorted(
        definition["fields"],
        key=lambda f: -TYPE_SIZES[f[0]],
    )

    fields = []
    offset = 0
    for field_type, name, count in ordered:
        size = TYPE_SIZES[field_type] * (count if count else 1)
        fields.append({
            "name": name,
            "type": TYPE_ENUM[field_type],
            "wire_type": field_type,
            "array_length": count,
            "offset": offset,
            "size": size,
        })
        offset += size

    return {
        "id": definition["id"],
        "name": definition["name"],
        "crc_extra": crc_extra_over(definition["name"], fields),
        "length": offset,
        "base_length": offset,
        "base_count": len(fields),
        "fields": fields,
        "local": True,
    }


def render(entries, dialect_version: str) -> str:
    out = []
    w = out.append
    w("#pragma once")
    w("")
    w("// FILE GENERATO. Non modificare a mano.")
    w("//")
    w("// Prodotto da tools/mavlink/generate_message_table.py a partire da")
    w(f"// pymavlink {dialect_version}, dialetto ardupilotmega.")
    w("//")
    w("// Contiene, per ogni messaggio interpretato da EFIS NEXT, il CRC_EXTRA e la")
    w("// disposizione dei campi sul filo. Sono dati definiti dal protocollo: qui non")
    w("// c'e' logica, e nessuna dipendenza viene introdotta a runtime. Il parser che")
    w("// li usa e' scritto nel progetto.")
    w("//")
    w("// Gli offset seguono l'ordine MAVLink, per dimensione decrescente, non quello")
    w("// di dichiarazione.")
    w("//")
    w("// `length` e' il payload completo, `base_length` la parte definita quando il")
    w("// messaggio e' nato. Un frame piu' corto di base_length ha semplicemente gli")
    w("// zeri finali rimossi, come il protocollo consente. Un frame che si ferma fra")
    w("// base_length e length proviene invece da un trasmettitore che le estensioni")
    w("// non le conosce: quei campi sono assenti, non nulli, e leggerli come zero")
    w("// significherebbe inventare un dato.")
    w("")
    w('#include "efis/providers/mavlink_types.hpp"')
    w("")
    w("namespace efis::mavlink::generated {")
    w("")

    for entry in entries:
        symbol = entry["name"].lower()
        extensions = entry["length"] - entry["base_length"]
        note = f", di cui {extensions} byte di estensione" if extensions else ""
        origin = ", definito nel progetto" if entry.get("local") else ""
        w(f"// {entry['name']} ({entry['id']}), payload {entry['length']} byte{note}{origin}")
        w(f"inline constexpr FieldDefinition {symbol}_fields[] = {{")
        for position, field in enumerate(entry["fields"]):
            tail = "  // estensione" if position >= entry["base_count"] else ""
            w(f'    {{"{field["name"]}", FieldType::{field["type"]}, '
              f'{field["array_length"]}, {field["offset"]}}},{tail}')
        w("};")
        w("")

    w("inline constexpr MessageDefinition messages[] = {")
    for entry in sorted(entries, key=lambda e: e["id"]):
        symbol = entry["name"].lower()
        w(f'    {{{entry["id"]}, "{entry["name"]}", {entry["crc_extra"]}, '
          f'{entry["length"]}, {entry["base_length"]}, {symbol}_fields, '
          f'{len(entry["fields"])}}},')
    w("};")
    w("")
    w("}  // namespace efis::mavlink::generated")
    w("")
    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    import pymavlink
    from pymavlink.dialects.v20 import ardupilotmega as dialect

    by_name = {c.msgname: c for c in dialect.mavlink_map.values()}
    missing = [name for name in MESSAGES if name not in by_name]
    if missing:
        raise SystemExit(f"messaggi assenti dal dialetto: {', '.join(missing)}")

    entries = [describe(by_name[name]) for name in MESSAGES]
    entries += [describe_local(definition) for definition in LOCAL_MESSAGES]

    duplicates = {e["id"] for e in entries if
                  sum(1 for other in entries if other["id"] == e["id"]) > 1}
    if duplicates:
        raise SystemExit(f"identificativi ripetuti: {sorted(duplicates)}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(entries, pymavlink.__version__),
                           encoding="utf-8", newline="\n")

    print(f"{len(entries)} messaggi scritti in {args.output}")
    for entry in sorted(entries, key=lambda e: e["id"]):
        extensions = entry["length"] - entry["base_length"]
        origin = "progetto" if entry.get("local") else "dialetto"
        print(f"  {entry['id']:>5}  {entry['name']:<24} crc_extra={entry['crc_extra']:<3} "
              f"base={entry['base_length']:<3} estensioni={extensions:<3} {origin}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
