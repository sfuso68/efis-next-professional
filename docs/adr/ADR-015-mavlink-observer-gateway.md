# ADR-015 — Gateway osservatori MAVLink senza bridge IP

- Stato: accettata; banco hardware parzialmente verificato, non flight approved
- Data: 2026-09-14
- Riferimenti: ADR-003, ADR-009, ADR-010, ADR-013,
  `MAVLINK_OBSERVER_GATEWAY.md`

## Contesto

Il Pixhawk 6X invia già MAVLink2 via Ethernet da `10.77.10.3` alla Blackbox
`10.77.10.2:14550`. Portatile Ground Station `192.168.1.39` e server
`192.168.1.6` devono poter osservare lo stesso flusso senza mantenere il
Pixhawk collegato via USB al portatile.

Collegare direttamente FLIGHT-DATA alla LAN, abilitare `ip_forward`, creare un
bridge L2 o applicare NAT esporrebbe anche Pixhawk, PFD, EMS e MFD ai sistemi
domestici. Questo annullerebbe l'isolamento e introdurrebbe un percorso di
comando non controllato verso l'autopilota.

## Decisione

La Blackbox non diventa un router. Il broker filtra l'ingresso MAVLink
sull'indirizzo sorgente Pixhawk configurato, conserva il normale percorso di
ingestion/recording e invia una copia byte-per-byte a un massimo di otto
endpoint UDP unicast dichiarati.

Il fan-out è strutturalmente solo trasmittente:

- non apre socket di ascolto sulla LAN di servizio;
- non legge risposte dai destinatari;
- non inoltra pacchetti verso Pixhawk;
- non interpreta né genera comandi MAVLink;
- rifiuta destinazioni duplicate, broadcast e wildcard;
- espone contatori separati per destinatario e isola gli errori.

La configurazione di banco autorizzata è:

```text
Pixhawk 10.77.10.3 --MAVLink2/UDP--> Blackbox 10.77.10.2:14550
                                            |
                                            +--TX-only--> 192.168.1.39:14550
                                            +--TX-only--> 192.168.1.6:14550
```

`net.ipv4.ip_forward` resta zero. Non esiste route, bridge o NAT fra
`10.77.10.0/24` e `192.168.1.0/24`.

Sul portatile una tabella nftables dedicata costituisce difesa in profondità:
ammette su UDP 14550 soltanto la sorgente Blackbox, scarta UDP in uscita verso
la Blackbox e vieta ogni destinazione `10.77.10.0/24`. La tabella non è la
garanzia primaria di non ritorno — che resta l'assenza del percorso nella
Blackbox — e la sua mancata attivazione mantiene l'acceptance in stato
`PARTIAL`.

QGroundControl viene avviato soltanto dopo un `ExecCondition` fail-closed che
verifica assenza USB/bridge, unicità della porta e identità SHA-256 del
pacchetto approvato. Un errore di condizione salta l'avvio senza restart-loop.

## Capacità e limite intenzionale

QGroundControl, MAVProxy o strumenti di sviluppo possono ricevere heartbeat e
telemetria. Il canale non consente download parametri completo, calibrazioni,
upload missione, cambio modalità, armamento o qualsiasi altra operazione che
richieda un ritorno verso Pixhawk.

Una futura modalità bidirezionale di manutenzione dovrà essere separata dal
servizio operativo, disabilitata al boot e subordinata almeno a interlock
fisico a terra, stato disarmato verificato, allowlist del computer, audit,
timeout, esclusione mutua col percorso operativo e rollback. Non è autorizzata
da questa ADR.

## Relazione con ADR-003 e ADR-010

ADR-003 resta invariata per display e software EFIS: solo la Blackbox interpreta
MAVLink e i pannelli ricevono DDP/EDP normalizzati. Questa ADR introduce una
sola eccezione controllata alla frase «soltanto oggetti applicativi validati»
di ADR-010: una copia grezza in sola uscita verso strumenti esterni di
osservazione. Il divieto di routing/bridging e di ritorno verso l'autopilota
resta pienamente valido.

## Gate hardware

- build ARM64 e test nativi sulla Blackbox;
- verifica `ip_forward=0` e assenza di bridge/NAT;
- cattura contemporanea su `eth0` e `wlan0` con sorgenti/destinazioni attese;
- ricezione sul portatile e sul server, senza duplicati dal vecchio USB;
- tentativo di comando dalla LAN con prova di zero pacchetti Blackbox→Pixhawk;
- fault di portatile/server/rete WLAN senza effetti su broker, recorder,
  DDP/EDP e controllo di volo;
- soak con misure perdita, jitter, CPU, memoria e temperatura.

Fino a queste prove lo stato resta `PARTIAL` e il sistema non è approvato per
il volo.
