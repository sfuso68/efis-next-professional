# Display Data Protocol V2

Stato: draft implementabile, versione wire `2.0`.

## Scopo

Trasportare verso renderer e strumenti solo stato normalizzato. Il protocollo non
trasporta oggetti Qt/LVGL né autorizza comandi di volo.

## Envelope

Ogni messaggio contiene:

| Campo | Tipo | Regola |
|---|---|---|
| `schema` | `u16` | valore `2` |
| `profile` | enum | `standard`, `professional`, `degraded` |
| `source_node` | `u32` | identità configurata |
| `sequence` | `u64` | crescente per publisher |
| `monotonic_us` | `u64` | tempo monotono publisher |
| `payload_type` | enum | snapshot, alert, heartbeat, config |
| `payload_length` | `u32` | limite da definire per trasporto |
| `payload_crc32c` | `u32` | obbligatorio su datagrammi/CAN |

## Flight display view

Il PFD riceve valori già convertiti nell'unità visualizzata con:

- valore;
- validità;
- source id;
- age;
- failure flag esplicito.

Campi minimi: roll, pitch, heading, IAS, altitude, VSI, ground speed, baro,
flight mode, selected altitude, selected course e health.

La guida di volo include stato FD, roll/pitch comandati e valori selezionati. Gli
allarmi sono ordinati dal core per severità e tempo; il renderer non ne cambia la
priorità e non genera autonomamente condizioni avioniche.

## Regole

1. Un consumer rifiuta schema maggiore non supportato.
2. Sequenze duplicate o arretrate vengono registrate e scartate.
3. Timeout locale porta il campo a failure; l'ultimo valore non resta verde.
4. `simulated` e `degraded` restano visibili.
5. Il renderer non arbitra le sorgenti.
6. La serializzazione wire verrà scelta con ADR separata dopo prove CAN/Ethernet.
