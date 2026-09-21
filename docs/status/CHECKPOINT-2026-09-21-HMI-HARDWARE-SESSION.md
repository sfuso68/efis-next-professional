# CHECKPOINT — HMI hardware acceptance session C8

Data: 2026-09-21
Branch: `test/hmi-hardware-collector-v1`
ID matrice: `AVX-G3-03`, `PRJ-004`
Classificazione: implementazione e verifica software; hardware non eseguito

## Implementato

- sessione nuova e non sovrascrivibile per PFD, MFD ed EMS;
- copia del piano A20 con SHA-256 bloccante;
- confronto del digest anche con il piano canonico del repository;
- source commit completo obbligatorio;
- tre record ruolo e directory artefatti separate;
- risultato iniziale `NOT_RUN` senza identità o misura inventata;
- path relativi confinati alla sessione e rifiuto degli escape;
- finalizzazione subordinata a tre run complete e conferma operatore esatta;
- riuso delle soglie e dei controlli P20 esistenti;
- assemblaggio atomico e validazione A20 indipendente;
- immutabilità degli output e `flight_approval=false` invariato;
- registrazione del test nel CTest/CI Linux.

## Verifica software

- 5/5 test del nuovo session manager: PASS;
- 3/3 test packager P20: PASS;
- 3/3 test validator A20: PASS;
- pipeline sintetica init → tre run → finalize → evidence → validator: PASS;
- mutazione del piano, sostituzione coordinata piano/digest, path escape,
  sessione incompleta, conferma errata e overwrite: rifiutati.

La fixture completa usa esclusivamente file sintetici e non costituisce
evidenza hardware.

## Hardware reale non verificato

- nessun nodo contattato;
- nessun pannello osservato o toccato;
- nessuna misura FPS, frame-time, termica o throttling acquisita;
- nessun cold boot, reboot, replay o fault Ethernet eseguito;
- nessuna firma operatore raccolta.

## Stato

`AVX-G3-03` resta `OPEN`; `PRJ-004` resta `VERIFY-HW`. Il prossimo passo è
creare una sessione dal commit installato, eseguire le otto famiglie di prova
su ciascun pannello e finalizzare soltanto dopo conferma dell'operatore.
