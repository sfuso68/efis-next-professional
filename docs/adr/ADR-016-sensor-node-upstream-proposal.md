# ADR-016 — proposta trasporto nodo sensori verso Blackbox

- Stato: proposta, non approvata e non eseguibile
- Data: 2026-09-19
- Riferimenti: ADR-003, ADR-005, ADR-010, ADR-011, ADR-013

## Contesto

EDP V1 descrive Blackbox → EMS e non può essere riutilizzato come scorciatoia
per il flusso RPi4 sensori → Blackbox. Il nodo sensori produce valori grezzi e
health di canale; Blackbox resta l'autorità che accetta calibrazione,
normalizza, arbitra, registra e pubblica lo snapshot motore ai display.

Il `SourceId` congelato non contiene una sorgente ASSO5 reale. Usare
`esp32_legacy`, `blackbox_broker` o un altro alias esistente falsificherebbe la
provenienza dei campioni.

## Proposta

- Riservare al nodo `sensor-rpi4` l'indirizzo statico `10.77.10.10`, senza
  configurarlo finché l'architettura non sarà approvata.
- Definire in un successivo freeze **ERAP V1**, Engine Raw Acquisition
  Protocol, UDP unicast one-way verso Blackbox `10.77.10.2:49004`.
- ERAP trasporta identità nodo/canale/sessione, sequenza, tempi, valore e unità
  grezzi, health, revisione candidata di calibrazione, revisione hardware e
  provenienza SIM/REAL. Non trasporta un valore engineering autorevole.
- Riservare append-only `SourceId::asso5_engine = 9`, senza modificare o usare
  l'ICD congelato prima di una review separata.
- Registrare il raw sia sul nodo sensori sia all'ingresso Blackbox; replay e
  normalizzazione devono usare lo stesso dataset.

## Vincoli

- Nessun comando, MAVLink, Pixhawk, display diretto, bridge, NAT o attuazione.
- Rate e freshness restano irrisolti fino alla caratterizzazione reale.
- Calibrazione ignota produce `UNAVAILABLE`, mai un valore apparentemente
  valido.
- La proposta non autorizza bind della porta, firewall, configurazione IP,
  servizio systemd o deployment.

## Blocco di capacità

Lo switch Waveshare installato ha cinque porte. Il piano già dichiara almeno
sei porte operative prima del nodo sensori; includendo `sensor-rpi4` ne servono
almeno sette. La topologia fisica deve quindi essere rivista prima di qualsiasi
deploy: non è accettabile eliminare ridondanze o collegamenti essenziali per
far entrare il nuovo nodo.

Il 2026-09-19 il proprietario ha autorizzato la sola registrazione documentale
del **MokerLink IND-F082GS** come candidato, sulla base del titolo commerciale
fornito. Il modello non è stato identificato da una targhetta reale e non è
installato. La configurazione dichiarata dal costruttore — otto RJ45 Fast
Ethernet, una RJ45 Gigabit e una SFP Gigabit — darebbe nove porte rame e quindi
un margine numerico di due porte rispetto al minimo di sette. Questo dato non
chiude il gate: identità, porte, velocità negoziate, alimentazione 12–55 V,
brownout, assorbimento, temperatura, riavvio e fault/recovery restano da
verificare sul dispositivo ricevuto.

Il candidato è ammesso esclusivamente per `FLIGHT-DATA`. Non sostituisce lo
switch VIDEO fisicamente separato, non deve collegare le telecamere e la porta
SFP non è conteggiata come rame senza un transceiver verificato. Le modalità
Extend/VLAN restano vietate finché non sono caratterizzate. Stato formale:
`CANDIDATE_NOT_PHYSICALLY_VERIFIED` e `CANDIDATE_CAPACITY_UNVERIFIED`.

## Decisione corrente

`NO_GO_PENDING_ARCHITECTURE_AND_HARDWARE_REVIEW`.

Questo ADR documenta una proposta tecnica sottoponibile a review, non una
decisione accettata.
