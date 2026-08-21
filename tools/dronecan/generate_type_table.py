#!/usr/bin/env python3
"""Genera la tabella dei tipi DroneCAN usata dal decodificatore di EFIS NEXT.

Il core non dipende da alcuna libreria DroneCAN, ne' in compilazione ne' a
runtime: il decodificatore e' scritto nel progetto. Restano pero' due cose che
non si possono inventare, perche' le definisce il protocollo e sbagliarle non
produce un errore visibile.

La prima e' la **signature** di ogni tipo, che entra nel CRC del trasferimento.
Un valore sbagliato non da' errore: fa scartare in silenzio tutti i trasferimenti
di quel tipo, esattamente come un CRC_EXTRA sbagliato in MAVLink.

La seconda e' la **disposizione dei campi in bit**. Il DSDL impacchetta i valori
senza allinearli ai byte — i giri motore sono un `uint17`, lo stato un `uint2` —
quindi un solo campo descritto male sposta tutti quelli che seguono.

Entrambe si estraggono qui dall'implementazione di riferimento e si scrivono in
un'intestazione leggibile e versionata. La generazione non fa parte della build.

Uso:

    python -m venv .venv && .venv/bin/pip install dronecan
    .venv/bin/python tools/dronecan/generate_type_table.py \\
        --output include/efis/providers/dronecan_type_table.hpp
"""

from __future__ import annotations

import argparse
import warnings
from pathlib import Path

warnings.filterwarnings("ignore")

# Tipi che EFIS NEXT interpreta. Sono quelli che coprono il nodo motore e lo
# stato dei nodi; gli altri si aggiungono qui quando serviranno.
TYPES = [
    "uavcan.equipment.ice.reciprocating.Status",
    "uavcan.equipment.ice.reciprocating.CylinderStatus",
    "uavcan.protocol.NodeStatus",
]

KIND_NAMES = {0: "boolean", 1: "unsigned", 2: "signed", 3: "float"}


def describe_fields(data_type):
    """Elenca i campi nell'ordine in cui stanno sul filo."""
    fields = []
    for field in data_type.fields:
        kind = field.type
        category = kind.category

        if category == kind.CATEGORY_VOID:
            fields.append({"name": "", "shape": "void", "bits": kind.bitlen})
            continue

        if category == kind.CATEGORY_PRIMITIVE:
            fields.append({
                "name": field.name,
                "shape": KIND_NAMES[kind.kind],
                "bits": kind.bitlen,
            })
            continue

        if category == kind.CATEGORY_ARRAY:
            element = kind.value_type
            # Un array a lunghezza variabile porta davanti il numero di elementi,
            # su tanti bit quanti ne servono a contare fino al massimo.
            fields.append({
                "name": field.name,
                "shape": "array",
                "bits": 0,
                "max_size": kind.max_size,
                "dynamic": kind.mode == kind.MODE_DYNAMIC,
                "element": element.full_name if element.category == element.CATEGORY_COMPOUND
                           else KIND_NAMES[element.kind],
                "element_bits": getattr(element, "bitlen", 0),
            })
            continue

        if category == kind.CATEGORY_COMPOUND:
            fields.append({
                "name": field.name,
                "shape": "compound",
                "bits": 0,
                "element": kind.full_name,
            })
            continue

        raise SystemExit(f"categoria di campo non gestita in {data_type.full_name}")
    return fields


def symbol(full_name: str) -> str:
    return full_name.replace(".", "_").lower()


def render(entries, version: str) -> str:
    out = []
    w = out.append
    w("#pragma once")
    w("")
    w("// FILE GENERATO. Non modificare a mano.")
    w("//")
    w("// Prodotto da tools/dronecan/generate_type_table.py a partire da")
    w(f"// dronecan {version}.")
    w("//")
    w("// Contiene, per ogni tipo interpretato, la signature che entra nel CRC del")
    w("// trasferimento e la disposizione dei campi in bit. Sono dati definiti dal")
    w("// protocollo: qui non c'e' logica, e nessuna dipendenza viene introdotta a")
    w("// runtime. Il decodificatore che li usa e' scritto nel progetto.")
    w("//")
    w("// Il DSDL non allinea i campi ai byte: le ampiezze sono in bit, e vanno lette")
    w("// nell'ordine in cui compaiono.")
    w("")
    w('#include "efis/providers/dronecan_types.hpp"')
    w("")
    w("namespace efis::dronecan::generated {")
    w("")

    for entry in entries:
        name = symbol(entry["name"])
        w(f"// {entry['name']}")
        w(f"inline constexpr FieldDescriptor {name}_fields[] = {{")
        for field in entry["fields"]:
            shape = {
                "void": "FieldShape::padding",
                "boolean": "FieldShape::boolean",
                "unsigned": "FieldShape::unsigned_integer",
                "signed": "FieldShape::signed_integer",
                "float": "FieldShape::floating",
                "array": "FieldShape::array",
                "compound": "FieldShape::compound",
            }[field["shape"]]
            label = field["name"] or "padding"
            extra = ""
            if field["shape"] == "array":
                extra = (f", {field['max_size']}, "
                         f"{'true' if field['dynamic'] else 'false'}")
            w(f'    {{"{label}", {shape}, {field["bits"]}{extra}}},')
        w("};")
        w("")

    w("inline constexpr TypeDescriptor types[] = {")
    for entry in sorted(entries, key=lambda e: e["dtid"] if e["dtid"] is not None else 1 << 20):
        name = symbol(entry["name"])
        dtid = entry["dtid"] if entry["dtid"] is not None else 0
        w(f'    {{{dtid}, "{entry["name"]}", 0x{entry["signature"]:016X}ULL, '
          f'{name}_fields, {len(entry["fields"])}}},')
    w("};")
    w("")
    w("}  // namespace efis::dronecan::generated")
    w("")
    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    import dronecan

    entries = []
    for name in TYPES:
        data_type = dronecan.TYPENAMES[name]
        entries.append({
            "name": name,
            "dtid": getattr(data_type, "default_dtid", None),
            "signature": data_type.get_data_type_signature(),
            "fields": describe_fields(data_type),
        })

    args.output.parent.mkdir(parents=True, exist_ok=True)
    version = getattr(dronecan, "__version__", "sconosciuta")
    args.output.write_text(render(entries, version), encoding="utf-8", newline="\n")

    print(f"{len(entries)} tipi scritti in {args.output}")
    for entry in entries:
        dtid = entry["dtid"] if entry["dtid"] is not None else "-"
        print(f"  {str(dtid):>6}  {entry['name']:<52} "
              f"signature 0x{entry['signature']:016X}  campi {len(entry['fields'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
