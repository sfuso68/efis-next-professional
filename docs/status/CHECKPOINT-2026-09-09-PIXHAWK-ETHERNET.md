# Checkpoint Pixhawk Ethernet e Emergency Assist — 2026-09-09

## Scopo del checkpoint

Questo documento congela lo stato raggiunto sul banco prima della pausa. Non
autorizza l'uso in volo e non modifica il confine read-only stabilito da
ADR-003 e ADR-010.

Branch: `work/avionics-ethernet-v1`

Commit applicativi di riferimento:

- `e916548` — registrazione delle transizioni di recupero del monitor nodi;
- `ee4ec64` — evidenza di build/deployment del monitor corretto.

Registro hardware consultato: `sfuso68/efis-evolved`, branch
`docs/hardware-registry-v1`, commit `4711fc1c9a8763b0eebae89f737e459b7992cec2`.

## Implementato

- Pixhawk 6X configurato come `10.77.10.3/24`, senza DHCP e senza default
  gateway, con MAVLink2/UDP unicast verso Blackbox `10.77.10.2:14550`.
- Bridge USB del portatile limitato a `127.0.0.1:14550`, così da non creare un
  secondo flusso indistinguibile verso la Blackbox.
- `NodeHealthMonitor` conserva ed emette anche la transizione
  `STALE/LOST -> ONLINE` generata da un heartbeat valido.
- Broker aggiornato sulla Blackbox con rollback locale disponibile.

## Verificato solo software

- Correzione del recovery event: build MSVC e 16/16 test superati.
- Lo stesso commit `e916548` ha superato build GCC 14.2 e 16/16 test nativi
  ARM64 sulla Blackbox.
- Lo smoke test isolato del broker ha prodotto 21 snapshot e 21 datagrammi,
  senza failure.

## Verificato su hardware reale

- Backup finale parametri Pixhawk completo 1084/1084, SHA-256
  `960f36c4ad5359b6eea43284ac2b2a681c289c298b794e120baefc14aa05e641`.
- Cattura di rete con byte iniziale `0xfd` e messaggi ArduPlane validi:
  MAVLink2 reale da `10.77.10.3` a `10.77.10.2:14550`.
- Primo distacco fisico del cavo Pixhawk: perdita ICMP completa, contatore
  MAVLink fermo a 1.951.512; broker, recorder, DDP e PFD sono rimasti attivi.
- Riconnessione: 4/4 ping, RTT medio 0,197 ms, traffico MAVLink2 e contatori
  broker nuovamente crescenti.
- Broker `e916548` installato e attivo, SHA-256
  `c621c272f2039123e7aee90232ae9ae3cc5488a11654873330ae15c79823c712`.
  Rollback: `efis-blackbox-broker.pre-e916548-20260908`.
- Dopo il deployment, MAVLink, CAN, recorder, EDP e DDP hanno continuato a
  crescere senza errori; il PFD è rimasto `LIVE`.

## Non verificato / aperto

- Il primo distacco fisico ha preceduto il deployment della correzione. Manca
  un secondo distacco/riconnessione con lettura del recorder che dimostri la
  sequenza esatta `STALE -> LOST -> ONLINE` sul broker reale.
- Manca un endurance test di almeno 60 minuti con misura di perdita, jitter,
  temperature, undervoltage e crescita recorder.
- Accelerometro 3D e bussola non hanno ancora superato le calibrazioni pre-arm.
- M10, ASPD-DLVR, mapping completo dei segnali e health DroneCAN non sono
  ancora chiusi.
- Nessun comando dal sistema EFIS verso il Pixhawk è implementato o autorizzato.

## Punto esatto di ripresa

1. Fotografare/registrare baseline di broker, recorder, PFD e contatori rete.
2. Scollegare soltanto il cavo Ethernet Pixhawk e attendere la soglia `LOST`.
3. Ricollegarlo e acquisire dal recorder l'evento `ONLINE` una sola volta.
4. Controllare che DDP/EDP e registrazione non abbiano interruzioni anomale.
5. Eseguire soak >=60 minuti e aggiornare `CORE-004`/`PIX-002` con evidenza.
6. Solo dopo la chiusura P2, aprire una milestone separata per eventuali
   comandi autopilota e banco HIL.

## Nuovo tema fissato, non implementato

È stato definito il concept `Emergency Assist / Panic V1`, descritto in
`docs/design/EMERGENCY_ASSIST_V1.md`. La V1 è consultiva e non ha autorità sui
comandi di volo. L'eventuale assistenza di stabilizzazione con servi di rollio
e beccheggio resta bloccata da identificazione hardware, interlock, HIL e prove
progressive.
