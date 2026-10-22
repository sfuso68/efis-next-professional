#!/usr/bin/env python3
"""Backup parametri: strumento ATTIVO di manutenzione, non osservatore RX-only.

Punto 3 delle priorita' P0 dell'handoff: creare un backup aggiornato dei
parametri del Pixhawk dopo gli interventi su GNSS, CAN e sensore aria. I backup
esistenti risalgono al 20 agosto 2026 e precedono quelle modifiche, quindi
oggi non descrivono piu' la configurazione reale.

Invia PARAM_REQUEST_LIST soltanto con --allow-parameter-request. Non invia
COMMAND_LONG, PARAM_SET o richieste stream e non tocca il firmware. Non usare
nel gateway osservatori ADR-015; il flag NON autorizza una sessione hardware.
La versione firmware viene soltanto ascoltata e puo' rimanere ignota.

Produce due file, non uno.

`mav.parm` e' nel formato di MAVProxy, lo stesso dei backup gia' presenti, cosi'
resta confrontabile con essi e ricaricabile con gli strumenti abituali.

`manifest.json` porta cio' che il formato MAVProxy non sa portare: quanti
parametri l'autopilota aveva dichiarato e quanti ne sono arrivati davvero, la
versione del firmware, l'impronta del contenuto e l'elenco degli indici mancanti.

La distinzione fra dichiarati e ricevuti e' la ragione per cui il manifesto
esiste. Un elenco di parametri arrivato incompleto ha esattamente l'aspetto di
un elenco completo: nessun campo dice che manca qualcosa. Ripristinare da un
backup del genere lascerebbe indietro proprio i parametri che non erano
arrivati, senza che nulla lo segnali. Qui un backup parziale e' dichiarato
parziale, e il nome del file lo riporta.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import time
import math
from datetime import datetime, timezone
from pathlib import Path

from pymavlink import mavutil


def version_string(value: int) -> str:
    return (f"{(value >> 24) & 0xFF}.{(value >> 16) & 0xFF}."
            f"{(value >> 8) & 0xFF}.{value & 0xFF}")


def format_value(value: float) -> str:
    """Scrive il valore nella forma piu' corta che si rilegge identica.

    MAVLink trasporta ogni parametro come float a 32 bit, anche quelli che
    ArduPilot dichiara interi. Scriverne nove cifre significative produce
    stringhe come 0.100000001 al posto di 0.1: il valore e' lo stesso, ma il
    confronto con una copia prodotta da MAVProxy segnala una differenza che non
    esiste.

    Non e' un problema estetico. La prima copia fatta con questo strumento
    differiva dal backup di fabbrica in settanta parametri, di cui quaranta
    erano soltanto questo. Un elenco di differenze in cui piu' della meta' e'
    rumore non viene letto con attenzione, ed e' esattamente l'elenco che
    dovrebbe far notare la riga che conta.

    Si cerca quindi la rappresentazione piu' breve che, riletta, produce di
    nuovo lo stesso float a 32 bit: nessuna cifra viene persa e nessuna viene
    inventata.
    """
    single = struct.unpack("<f", struct.pack("<f", value))[0]
    for digits in range(1, 10):
        text = f"{single:.{digits}g}"
        if struct.unpack("<f", struct.pack("<f", float(text)))[0] == single:
            return text
    return f"{single:.9g}"


def decode_name(raw) -> str:
    if isinstance(raw, (bytes, bytearray)):
        return bytes(raw).rstrip(b"\x00").decode("ascii", "replace")
    return str(raw).rstrip("\x00")


def collect(link, timeout: float, quiet_after: float, *, allow_request=False):
    """Raccoglie i parametri fino al conto dichiarato o al silenzio.

    Due criteri di uscita, perche' nessuno dei due basta da solo: il conto
    dichiarato non arriva se il primo messaggio si perde, e il silenzio da solo
    scambierebbe una pausa per la fine dell'elenco.
    """
    if not allow_request:
        raise PermissionError("active PARAM_REQUEST_LIST requires explicit consent")
    link.mav.param_request_list_send(link.target_system, link.target_component)

    values: dict[str, float] = {}
    indices: dict[str, int] = {}
    declared = None
    deadline = time.monotonic() + timeout
    last_message = time.monotonic()

    while time.monotonic() < deadline:
        message = link.recv_match(type="PARAM_VALUE", blocking=True, timeout=0.5)
        if message is None:
            if values and time.monotonic() - last_message > quiet_after:
                break
            continue
        last_message = time.monotonic()
        name = decode_name(message.param_id)
        if not name:
            continue
        values[name] = float(message.param_value)
        indices[name] = int(message.param_index)
        declared = int(message.param_count)
        if declared and len(values) >= declared:
            break

    return values, indices, declared


def firmware_of(link, timeout: float):
    """Listen only; a missing unsolicited version is not a backup failure."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        message = link.recv_match(type="AUTOPILOT_VERSION", blocking=True, timeout=0.5)
        if message is not None:
            return {
                "flight_sw": version_string(message.flight_sw_version),
                "board_version": int(message.board_version),
                "vendor_id": int(message.vendor_id),
                "product_id": int(message.product_id),
                "uid2": bytes(message.uid2).hex(),
                "flight_custom_version": bytes(message.flight_custom_version).hex(),
            }
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output-dir", type=Path, required=True,
                        help="cartella di destinazione; ne viene creata una "
                             "sottocartella con l'istante della copia")
    parser.add_argument("--label", default="backup",
                        help="etichetta che compare nel nome della sottocartella")
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--quiet-after", type=float, default=4.0)
    parser.add_argument("--allow-parameter-request", action="store_true",
                        help="explicitly permit active maintenance PARAM_REQUEST_LIST; "
                             "not permission to operate hardware")
    args = parser.parse_args()
    if not args.allow_parameter_request:
        parser.error("active maintenance requires --allow-parameter-request; no connection opened")
    if any(not math.isfinite(value) or value <= 0
           for value in (args.timeout, args.quiet_after)):
        parser.error("timeouts must be finite and positive")

    link = mavutil.mavlink_connection(args.device, baud=args.baud, source_system=254)
    try:
        if link.wait_heartbeat(timeout=15) is None:
            raise SystemExit("nessun battito MAVLink: autopilota non raggiungibile")
        firmware = firmware_of(link, 5.0)
        values, indices, declared = collect(
            link, args.timeout, args.quiet_after, allow_request=True)
    finally:
        link.close()
    if not values:
        raise SystemExit("nessun parametro ricevuto: copia non effettuata")

    received = len(values)
    complete = declared is not None and received == declared

    missing = []
    if declared:
        present = set(indices.values())
        missing = [i for i in range(declared) if i not in present]

    body = "".join(f"{name:<16} {format_value(values[name])}\n" for name in sorted(values))
    digest = hashlib.sha256(body.encode("ascii")).hexdigest()

    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    suffix = "" if complete else "-PARZIALE"
    target = args.output_dir / f"{args.label}-{stamp}{suffix}"
    target.mkdir(parents=True, exist_ok=True)

    (target / "mav.parm").write_text(body, encoding="ascii")

    manifest = {
        "schema": "efis-next.parameter-backup.v1",
        "capture_mode": "ACTIVE_MAINTENANCE_PARAMETER_REQUEST",
        "transmitted_messages": ["PARAM_REQUEST_LIST"],
        "firmware_status": "OBSERVED" if firmware else "NOT_OBSERVED",
        "captured_utc": datetime.now(timezone.utc).isoformat(),
        "device": args.device,
        "target_system": link.target_system,
        "target_component": link.target_component,
        "firmware": firmware,
        "declared_count": declared,
        "received_count": received,
        "complete": complete,
        "missing_indices": missing[:64],
        "missing_indices_truncated": len(missing) > 64,
        "sha256_mav_parm": digest,
        "status": "DEVELOPMENT / NON-CERT",
    }
    (target / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True), encoding="utf-8")

    print(json.dumps(manifest, indent=2, sort_keys=True))
    if not complete:
        print(f"\nATTENZIONE: copia parziale, {received} parametri su "
              f"{declared if declared is not None else 'ignoti'}. "
              "La cartella e' marcata PARZIALE e non va usata per un ripristino.")
        return 1
    print(f"\nCopia completa: {received} parametri in {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
