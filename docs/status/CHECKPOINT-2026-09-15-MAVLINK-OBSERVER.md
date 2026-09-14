# EFIS NEXT — Checkpoint MAVLink Observer Gateway

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: DEVELOPMENT / NON-CERT / NON FLIGHT APPROVED

## Baseline fissata

- `f62ec89` — unità QGroundControl permanente `MONITOR ONLY`, preflight e
  launcher;
- `a94c476` — harness recuperabile per il fault Wi-Fi della Ground Station;
- Pixhawk `10.77.10.3` invia via Ethernet alla sola Blackbox
  `10.77.10.2:14550`;
- Blackbox trasmette copie applicative TX-only a portatile
  `192.168.1.39:14550` e server `192.168.1.6:14550`;
- non esistono bridge, NAT, IP forwarding o socket applicativi di ritorno
  verso Pixhawk.

## Dieci passaggi eseguiti

1. Letti stato autorevole, handoff, checkpoint precedente e ADR-015; worktree
   pulita salvo gli artefatti `output/*` dell'utente.
2. Acquisita baseline: QGC attivo, bridge USB inattivo, nessun `/dev/ttyACM*`,
   Blackbox e sink server `active/running`, entrambi `NRestarts=0`.
3. Il primo tentativo di port-holder inline è fallito per quoting Python ed è
   stato escluso dalle evidenze; QGC era ripartito normalmente.
4. Aggiunto `udp_port_holder.py`, strumento RX-only/no-transmit deterministico,
   con test automatico di occupazione e rilascio della porta.
5. Eseguito un conflitto reale su `0.0.0.0:14550`: il solo holder Python era
   in bind e il preflight QGC ha prodotto `status=BLOCKED` con
   `udp_listener_available=false`.
6. Il test ha scoperto che `RestartPreventExitStatus=78` non ferma i retry di
   un errore proveniente da `ExecStartPre`: prima della correzione sono stati
   osservati dieci retry. Il servizio è stato arrestato immediatamente.
7. Il gate è stato spostato in `ExecCondition`, con test di regressione sulla
   unità. Ripetendo il conflitto: `Result=exec-condition`, servizio
   `inactive/dead`, `NRestarts=0` e nessun processo QGC avviato.
8. Durante l'assenza di QGC la Blackbox è rimasta `active/running`, senza
   restart o errori. Tra baseline e controllo finale MAVLink è cresciuto da
   164.511 a 176.137 datagrammi, il recorder da 155.296 a 164.147 record e
   ciascun display da 155.296 a 164.147 datagrammi.
9. Il sink server è rimasto `LIVE`, `NRestarts=0`, passando da 61.607 a 73.206
   frame e da 2.344 a 2.785 heartbeat, con zero malformed, source rejection e
   socket error.
10. Alla scadenza dell'holder la porta è stata rilasciata. Il preflight è
    tornato `PASS` e QGC è ripartito al primo tentativo, PID 47330, unico bind
    UDP 14550 e `NRestarts=0`.

## Implementato ma verificato solo software

- `udp_port_holder.py` con intervallo ammesso 1–600 secondi, bind esplicito,
  log JSON ready e nessuna API di trasmissione;
- test subprocess che prova collisione reale e rilascio dell'endpoint;
- test di regressione che impone `ExecCondition` e vieta il ritorno a
  `ExecStartPre` per il preflight;
- harness Wi-Fi con recovery-before-disconnect; `/bin/sh -n` PASS e rifiuto
  senza privilegi verificato.

## Verificato su hardware reale

- conflitto locale UDP 14550 sul portatile Debian `Port-hp`;
- blocco fail-closed senza retry dopo la correzione;
- continuità Blackbox, recorder, due display e sink server durante il fault;
- rilascio automatico del conflitto e ripartenza QGC con triplo preflight
  `PASS`.

## Aperto — non dichiarare DONE

- il fault eseguito riguarda il consumer/endpoint locale, non la perdita
  completa del portatile o della WLAN;
- eseguire dalla console locale, con autorizzazione amministrativa:
  `sudo /home/stefano/fault_laptop_wifi.sh wlo1 CASA 90`;
- eseguire il secondo reboot server per provare il bind-wait nella condizione
  di boot e acquisire il contatore nft post-reboot;
- provare login/reboot grafico del portatile e avvio automatico QGC;
- QGC 5.0.8 richiede ancora SIGKILL dopo otto secondi e segnala `speechd`
  mancante;
- sostituire l'alimentazione USB da banco del Pixhawk con POWER1 regolata;
- eseguire soak con perdita/jitter, risorse e temperature.

Questo checkpoint non autorizza comando, configurazione o manutenzione
bidirezionale del Pixhawk attraverso la Blackbox.
