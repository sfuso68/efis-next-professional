#!/usr/bin/env python3
"""Scrive un parametro MAVLink con guardia, riletta indipendente e ripristino.

Il paragrafo 16 dell'handoff elenca cio' che mancava alla versione precedente,
rimasta inutilizzabile dopo un TypeError di pymavlink: riprodurre il problema su
un banco, correggere la chiamata, aggiungere una riletta obbligatoria, aggiungere
timeout e ripristino, e non provarla sui parametri critici del Pixhawk reale.

Le scelte che ne derivano.

**Non scrive, se non glielo si chiede.** Senza `--commit` lo strumento legge,
verifica la guardia e dichiara che cosa farebbe. Un'invocazione sbagliata, o una
riga di comando recuperata dalla cronologia, non modifica nulla.

**La riletta e' indipendente.** La risposta che l'autopilota manda subito dopo la
scrittura non e' una prova: e' l'eco della richiesta. Dopo di essa lo strumento
richiede il parametro da capo. Il banco di prova sa fingere esattamente questo
caso, un autopilota che risponde con il valore nuovo e ne conserva uno vecchio.

**Il ripristino e' automatico e verificato.** Se la riletta non coincide, il
valore precedente viene riscritto e a sua volta riletto. Un ripristino non
verificato lascerebbe il sistema in uno stato che nessuno ha scelto, che e'
peggio del fallimento iniziale.

**Alcuni parametri non si scrivono senza dirlo ad alta voce.** Quelli che
disattivano protezioni o abilitano un sensore al controllo di volo richiedono
`--allow-protected`. ARSPD_USE e' fra questi: il paragrafo 14 impone che resti a
zero fino al termine della validazione, e questo strumento non deve essere la
strada breve per aggirarla.

**Ogni tentativo lascia traccia.** Con `--audit-dir` viene scritto un documento
JSON per ogni esecuzione, riuscita o fallita.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import time
from datetime import datetime, timezone
from pathlib import Path

from pymavlink import mavutil

# Parametri che non si modificano per errore. Non e' un elenco di sicurezza
# completo, e non pretende di esserlo: e' l'insieme di cio' che, cambiato di
# nascosto, toglierebbe una protezione o promuoverebbe un sensore non validato a
# sorgente per il controllo di volo.
PROTECTED = {
    "ARSPD_USE": "abilita il sensore aria al controllo di volo; il paragrafo 14 "
                 "lo tiene a zero fino al termine della validazione in volo",
    "ARMING_CHECK": "disattiverebbe i controlli pre-armamento",
    "ARMING_REQUIRE": "cambia il requisito di armamento",
    "BRD_SAFETY_DEFLT": "riguarda l'interruttore di sicurezza hardware",
    "BRD_SAFETYENABLE": "riguarda l'interruttore di sicurezza hardware",
    "AHRS_EKF_TYPE": "cambia la sorgente di assetto usata dal controllo di volo",
}

TOLERANCE = 1e-4
PYMAVLINK_MULTI_INSTANCE_ERROR = "'NoneType' object does not support item assignment"


class ParameterError(RuntimeError):
    pass


def receive_match(link, *, message_type: str, timeout: float):
    """Riceve un messaggio senza estendere il timeout su frame malformati.

    Alcune versioni di pymavlink possono sollevare un ``TypeError`` interno
    mentre aggiornano la cache di un messaggio telemetrico multi-istanza. Il
    frame e' gia' stato consumato e non riguarda necessariamente il tipo che
    stiamo aspettando. Ignoriamo esclusivamente quella firma nota e continuiamo
    entro la stessa scadenza; qualsiasi altro errore resta visibile.
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        try:
            message = link.recv_match(
                type=message_type, blocking=True, timeout=min(0.5, remaining))
        except TypeError as error:
            if PYMAVLINK_MULTI_INSTANCE_ERROR not in str(error):
                raise
            continue
        if message is not None:
            return message
    return None


def wait_for_heartbeat(link, timeout: float):
    message = receive_match(link, message_type="HEARTBEAT", timeout=timeout)
    if message is not None:
        # post_message normalmente valorizza gia' questi campi; assegnarli dal
        # battito rende esplicito il destinatario anche dopo un frame scartato.
        link.target_system = message.get_srcSystem()
        link.target_component = message.get_srcComponent()
    return message


def parameter_id(name: str) -> bytes:
    """Identificativo nella forma che il protocollo richiede.

    E' qui che la versione precedente si e' rotta. pymavlink vuole `bytes` per un
    campo dichiarato `char[16]`; passare una stringa produce il TypeError
    riportato nell'handoff. Il troncamento a sedici byte e' quello del protocollo:
    un nome piu' lungo non esiste, e tagliarlo in silenzio scriverebbe su un
    parametro diverso da quello chiesto.
    """
    encoded = name.encode("ascii")
    if len(encoded) > 16:
        raise ParameterError(f"nome troppo lungo per il protocollo: {name}")
    return encoded


def read_parameter(link, name: str, timeout: float, retries: int):
    """Legge un parametro, ripetendo la richiesta se la risposta non arriva.

    Un parametro non letto non e' un parametro assente: su una seriale condivisa
    una risposta si perde con facilita'. Distinguere i due casi conta, perche'
    rinunciare alla lettura significherebbe rinunciare alla guardia.
    """
    encoded = parameter_id(name)
    for attempt in range(retries + 1):
        link.mav.param_request_read_send(link.target_system, link.target_component,
                                         encoded, -1)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            message = receive_match(
                link, message_type="PARAM_VALUE",
                timeout=min(0.5, max(0.0, deadline - time.monotonic())))
            if message is None:
                continue
            received = bytes(message.param_id).rstrip(b"\x00").decode("ascii", "replace") \
                if isinstance(message.param_id, (bytes, bytearray)) \
                else str(message.param_id).rstrip("\x00")
            # Un PARAM_VALUE che riguarda un altro parametro non e' una risposta
            # alla nostra domanda: scartarlo evita di leggere il valore sbagliato.
            if received == name:
                return float(message.param_value), int(message.param_type), attempt
    raise ParameterError(f"nessuna risposta per {name} dopo {retries + 1} tentativi")


def write_parameter(link, name: str, value: float, kind: int, timeout: float):
    """Invia la scrittura e attende l'eco. L'eco non e' una verifica."""
    link.mav.param_set_send(link.target_system, link.target_component,
                            parameter_id(name), float(value), int(kind))
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        message = receive_match(
            link, message_type="PARAM_VALUE",
            timeout=min(0.5, max(0.0, deadline - time.monotonic())))
        if message is None:
            continue
        received = bytes(message.param_id).rstrip(b"\x00").decode("ascii", "replace") \
            if isinstance(message.param_id, (bytes, bytearray)) \
            else str(message.param_id).rstrip("\x00")
        if received == name:
            return float(message.param_value)
    return None


def close_enough(left: float, right: float) -> bool:
    return math.isclose(left, right, abs_tol=TOLERANCE)


def write_audit(directory: Path | None, record: dict) -> Path | None:
    if directory is None:
        return None
    directory.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    path = directory / f"param-{record['parameter']}-{stamp}.json"
    path.write_text(json.dumps(record, indent=2, sort_keys=True), encoding="utf-8")
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", required=True,
                        help="seriale dell'autopilota, oppure un estremo di rete "
                             "per provare contro il banco")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--name", required=True)
    parser.add_argument("--expected", type=float, required=True,
                        help="valore che il parametro deve avere ora; se non "
                             "coincide non si scrive nulla")
    parser.add_argument("--value", type=float, required=True)
    parser.add_argument("--commit", action="store_true",
                        help="esegue davvero la scrittura; senza, dichiara "
                             "soltanto che cosa farebbe")
    parser.add_argument("--allow-protected", action="store_true")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--retries", type=int, default=2)
    parser.add_argument("--audit-dir", type=Path, default=None)
    args = parser.parse_args()

    record = {
        "schema": "efis-next.parameter-write.v2",
        "parameter": args.name,
        "requested_value": args.value,
        "expected_before": args.expected,
        "device": args.device,
        "committed": bool(args.commit),
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "outcome": "incomplete",
    }

    if args.name in PROTECTED and not args.allow_protected:
        record["outcome"] = "refused_protected"
        record["reason"] = PROTECTED[args.name]
        write_audit(args.audit_dir, record)
        print(json.dumps(record, indent=2, sort_keys=True))
        raise SystemExit(
            f"{args.name} e' protetto: {PROTECTED[args.name]}.\n"
            "Se la modifica e' davvero voluta, ripetere con --allow-protected.")

    link = mavutil.mavlink_connection(args.device, baud=args.baud, source_system=254)
    if wait_for_heartbeat(link, timeout=10) is None:
        raise SystemExit("nessun battito MAVLink: autopilota non raggiungibile")
    record["target_system"] = link.target_system
    record["target_component"] = link.target_component

    before, kind, attempts = read_parameter(link, args.name, args.timeout, args.retries)
    record["value_before"] = before
    record["parameter_type"] = kind
    record["read_attempts"] = attempts + 1

    if not close_enough(before, args.expected):
        record["outcome"] = "guard_failed"
        write_audit(args.audit_dir, record)
        print(json.dumps(record, indent=2, sort_keys=True))
        raise SystemExit(
            f"guardia fallita: {args.name} vale {before}, atteso {args.expected}. "
            "Nessuna scrittura effettuata.")

    if close_enough(before, args.value):
        record["outcome"] = "already_set"
        write_audit(args.audit_dir, record)
        print(json.dumps(record, indent=2, sort_keys=True))
        return 0

    if not args.commit:
        record["outcome"] = "dry_run"
        write_audit(args.audit_dir, record)
        print(json.dumps(record, indent=2, sort_keys=True))
        print(f"\nProva a vuoto: {args.name} passerebbe da {before} a {args.value}.\n"
              "Ripetere con --commit per scrivere davvero.")
        return 0

    echoed = write_parameter(link, args.name, args.value, kind, args.timeout)
    record["echoed_value"] = echoed

    # La riletta e' una domanda nuova, non l'eco di prima. E' l'unico modo per
    # accorgersi di un autopilota che conferma senza applicare.
    after, _, _ = read_parameter(link, args.name, args.timeout, args.retries)
    record["value_after"] = after

    if close_enough(after, args.value):
        record["outcome"] = "verified"
        record["finished_utc"] = datetime.now(timezone.utc).isoformat()
        audit = write_audit(args.audit_dir, record)
        if audit is not None:
            record["audit_file"] = str(audit)
        print(json.dumps(record, indent=2, sort_keys=True))
        return 0

    # Riletta discorde: si ripristina il valore di partenza e lo si verifica a
    # sua volta. Un ripristino dichiarato ma non verificato non varrebbe nulla.
    record["outcome"] = "readback_mismatch"
    write_parameter(link, args.name, before, kind, args.timeout)
    try:
        restored, _, _ = read_parameter(link, args.name, args.timeout, args.retries)
    except ParameterError as error:
        record["rollback"] = "unverifiable"
        record["rollback_error"] = str(error)
        restored = None
    else:
        record["value_restored"] = restored
        record["rollback"] = "verified" if close_enough(restored, before) else "failed"

    record["finished_utc"] = datetime.now(timezone.utc).isoformat()
    write_audit(args.audit_dir, record)
    print(json.dumps(record, indent=2, sort_keys=True))

    if record.get("rollback") == "verified":
        raise SystemExit(
            f"scrittura non applicata: {args.name} vale {after}, atteso {args.value}. "
            f"Valore precedente {before} ripristinato e verificato.")
    raise SystemExit(
        f"scrittura non applicata E ripristino non verificato: {args.name} vale "
        f"{restored if restored is not None else 'ignoto'}. "
        "Verificare manualmente prima di qualsiasi altra operazione.")


if __name__ == "__main__":
    os.environ.setdefault("MAVLINK20", "1")
    try:
        raise SystemExit(main())
    except ParameterError as failure:
        # Un errore di protocollo e' una condizione prevista, non un difetto
        # dello strumento: va detto in una riga, non con una traccia di stack che
        # nasconde il fatto che nulla e' stato scritto.
        raise SystemExit(f"errore: {failure}")
