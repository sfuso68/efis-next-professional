# ADR-009 — Trasporto UDP unicast per DDP V2

- Stato: accettata per sviluppo e prove hardware
- Data: 2026-09-06
- Riferimenti: ADR-003, ADR-008, `DISPLAY_DATA_PROTOCOL_V2.md`

## Decisione

Gli snapshot periodici DDP V2 viaggiano dalla Blackbox ai display su UDP
unicast IPv4. Ogni display ha un endpoint configurato; non si usano broadcast o
multicast nella prima implementazione. Il PFD ascolta inizialmente sulla porta
`49002` e può vincolare il consumer al `source_node` Blackbox previsto.

UDP è un adapter di trasporto, non entra nel core né nel codec. Il publisher
produce uno snapshot completo a ogni invio; il consumer privilegia quindi lo
stato più recente e non attende la ritrasmissione di uno stato ormai superato.
CRC32C, sequenza, identità di istanza, gap e timeout restano responsabilità DDP.

Il percorso è unidirezionale. Il receiver del display non contiene alcuna API
per inviare comandi alla Blackbox o al controllo di volo.

## Motivazione

- uno snapshot perso non deve bloccare quelli successivi;
- latenza e jitter sono più importanti della consegna ritardata di ogni frame;
- il confine datagramma coincide con quello del messaggio DDP;
- il comportamento è riproducibile su loopback e misurabile sulla LAN reale;
- unicast limita la distribuzione ai display configurati.

## Limiti e gate

UDP, CRC e `source_node` non autenticano il mittente. Prima di un impiego
professionale servono una rete avionica cablata e isolata oppure autenticazione
crittografica del messaggio, provisioning delle chiavi e gestione del replay.
La porta deve essere filtrata alle sole interfacce e agli indirizzi previsti.

La prova software loopback non chiude il gate hardware. Restano obbligatori:

- misura di perdita, riordino, latenza e jitter Blackbox → RPi5;
- scollegamento cavo e verifica `LIVE → STALE → LOST` sul display reale;
- reboot del publisher e rifiuto dei pacchetti dell'istanza precedente;
- carico simultaneo CAN, MAVLink, recorder e rendering;
- conferma che nessun pacchetto esca dal display verso Pixhawk o Blackbox.
