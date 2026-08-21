# ADR-007: DroneCAN sul bus sensori

- Stato: accettata
- Data: 2026-08-21
- Sostituisce: la sezione "Perché non DroneCAN" di `ENGINE_CAN_V1`

## Contesto

Il progetto si è trovato con **due protocolli CAN scritti a mano nello stesso
giorno**: `ENGINE_CAN_V1` per il nodo motore ed `ESP32_CAN_V1` per il nodo
assetto, dati aria e satellitare. Entrambi assegnano gli identificatori a mano.

Si scontrano già. `ENGINE_CAN_V1` compone l'identificatore come
`(nodo << 7) | messaggio`, quindi il nodo 2 occupa `0x101`–`0x17F`; `0x101` è
`AIR_DATA` nell'altro schema. Due protocolli, poche ore, e la collisione è
arrivata da sola: mantenere a mano un registro di identificatori non è un lavoro
che questo progetto può permettersi di fare bene.

Il motivo decisivo però è un altro. **Il Pixhawk è collegato al bus.** Un formato
nostro lo capisce soltanto EFIS NEXT; un formato DroneCAN lo capisce anche
ArduPilot, che quindi vede il motore e non solo lo vede il nostro display.

## Decisione

Il bus sensori adotta **DroneCAN v0**, non Cyphal: è quello che parla il Matek
ASPD-DLVR già installato su CAN1 ed è quello maturo in ArduPilot. Cyphal è
incompatibile e non porta nulla che qui serva.

Si usano i tipi standard, senza inventarne di nuovi:

| Tipo | dtid | Contenuto |
|---|---:|---|
| `uavcan.equipment.ice.reciprocating.Status` | 1120 | giri, aspirazione, olio, carburante, e per cilindro **CHT ed EGT** |
| `uavcan.equipment.ice.FuelTankStatus` | 1129 | serbatoi |
| `uavcan.equipment.power.CircuitStatus` | 1091 | tensione e corrente |
| `uavcan.protocol.NodeStatus` | 341 | uptime, salute, modo |
| `uavcan.equipment.air_data.RawAirData` | 1027 | dati aria |
| `uavcan.equipment.gnss.Fix2` | 1063 | satellitare |
| `uavcan.equipment.ahrs.Solution` | 1000 | assetto |

Entrambi i protocolli fatti a mano reinventavano messaggi che esistono già,
compreso il pezzo che sembrava più specifico: CHT ed EGT per cilindro stanno in
`CylinderStatus`, insieme a lambda, anticipo e tempo di iniezione.

## Verifica fatta prima di decidere

Non è un ragionamento: è stato provato sul bus. Un
`ice.reciprocating.Status` con quattro cilindri è stato costruito con
l'implementazione di riferimento, iniettato su `vcan0` del nodo black box con
`cansend`, ripreso con `candump` e riassemblato.

| Misura | Valore |
|---|---|
| Frame per un solo messaggio motore | **11** |
| Byte utili trasportati | 77 |
| Identificatore | `0x1404601F` — priorità 20, dtid 1120, nodo 31 |
| Signature del tipo | `0xD38AA3EE75537EC6` |
| Esito del riassemblaggio | tutti i campi recuperati |

La registrazione è in `tests/data/can/dronecan-ice-status-vcan0-20260821.log` e
diventa il vettore di riferimento del decodificatore.

## Conseguenze

**Il costo è reale e va detto.** Un messaggio motore non entra in un frame: serve
il trasferimento multi-frame, con transfer ID, bit di toggle, CRC sul
trasferimento e riassemblaggio. Il DSDL è impacchettato a bit e non allineato ai
byte come MAVLink — i giri sono un `uint17`, lo stato un `uint2` — quindi il
decodificatore è più complesso di quello scritto per `ENGINE_CAN_V1`, non meno.

**Si perde risoluzione.** Molti campi sono `float16`. Nella prova, 178.0 °C sono
tornati 178.1, 742.0 sono tornati 741.9, e 496.4 kPa sono tornati 496.5. Il
protocollo fatto in casa, con interi scalati, era esatto a 0.1 °C. La perdita è
accettabile per CHT ed EGT, dove un quarto di grado non cambia una decisione, ma
va conosciuta e non scoperta in volo.

**Le unità non sono quelle del cruscotto.** Il DSDL usa kelvin e kilopascal,
mentre il PFD mostra gradi Celsius, psi e pollici di mercurio. È un argomento in
più per la regola che già seguiamo: il campione conserva il valore grezzo accanto
a quello convertito, e la conversione si può rifare rileggendo una registrazione.

**Non si perde il lavoro fatto.** La struttura dell'adapter resta: provenienza
completa su ogni campione, salute dei nodi, sonda illeggibile che non diventa
zero, contatore esteso, rifiuto dei frame malformati. Cambia il decodificatore
sotto.

**Il metodo resta quello che ha funzionato per MAVLink**: le definizioni si
generano dall'implementazione di riferimento invece di trascriverle, e si
verificano contro frame veri. È così che è stato trovato l'errore del bus
condiviso, ed è così che va trovato il prossimo.

## Cosa resta di ENGINE_CAN_V1

Il documento resta come **cronaca di una scelta superata**, marcato tale, per due
ragioni concrete: contiene l'analisi delle grandezze che il nodo motore deve
misurare, che vale indipendentemente dal formato; e la sua sezione "Perché non
DroneCAN" è la motivazione che questa ADR corregge, e cancellarla nasconderebbe
il fatto che è stata scritta.

## Limiti correnti

Il percorso di sola lettura non cambia: nessun componente di EFIS NEXT trasmette
verso il bus. DroneCAN prevede l'allocazione dinamica degli identificatori di
nodo, che è una trasmissione: finché non sarà deciso diversamente, i nodi useranno
identificatori statici e nessuno li chiederà al bus.

Resta da misurare il carico del bus con tutti i nodi presenti: undici frame per
messaggio motore, a dieci hertz, sono 110 frame al secondo per il solo motore.
Va confrontato con la capacità reale a 1 Mbit/s prima di fissare le cadenze.
