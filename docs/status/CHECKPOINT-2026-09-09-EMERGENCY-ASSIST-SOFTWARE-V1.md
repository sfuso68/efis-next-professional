# Checkpoint Emergency Assist Software V1 — 2026-09-09

Branch: `work/avionics-ethernet-v1`

## Confine

Questo checkpoint implementa un banco consultivo. Non autorizza uso in volo,
non contiene una checklist dell'esemplare e non introduce alcun comando verso
Pixhawk, servi, radio, transponder, flap, carrello o impianti.

## Implementato

- core C++20 indipendente da Qt con otto famiglie di emergenza;
- macchina a stati e conferme esplicite del pilota;
- rilevazione perdita potenza multi-segnale, con dwell, inerte senza
  configurazione approvata e capace soltanto di produrre `SUSPECTED`;
- gate fail-closed per dati primari, AGL, posizione, vento, terreno, ostacoli,
  database aeroporti, performance e procedure;
- planata conservativa e ranking candidati soltanto dopo tutti i gate;
- distinzione `SIM_ESTIMATE`/`REACHABLE` e divieto di best glide numerica non
  approvata;
- package checklist con ID, revisione, fonte e hash obbligatori;
- eventi deterministici per attivazione, conferma, step, skip, repeat, recupero
  e cancellazione;
- pagina PFD 1024×600 FLY/NAVIGATE/COMMUNICATE/MANAGE;
- accesso touch a pressione prolungata e scorciatoia F12 di banco;
- scenario `engine-emergency` con dati e passaggi di test marcati SIM;
- spool JSONL locale; ADR-012 e contratto Emergency Event V1.

## Verificato solo software

- compilazione MSVC 19.51 del core, PFD Qt 6.11 e QML AOT: PASS;
- `efis_emergency_assist_tests`: PASS;
- tutte le otto famiglie richiedono conferma manuale nel test di tassonomia;
- avvio PFD Qt offscreen con `--emergency-preview`: exit code 0;
- cattura 1024×600 prodotta: geometria generale senza collisioni dei pannelli;
  il backend Windows sostituisce i glifi e non verifica la leggibilità reale.
- build completa Windows riuscita; CTest globale non concluso perché Windows
  Application Control ha bloccato cinque eseguibili esistenti e il launcher è
  arrivato al timeout. Non viene dichiarato un PASS globale.

## Verificato su hardware reale

Niente per Emergency Assist. La release PFD installata sul RPi5 è ancora
`74a85f9` e non contiene questo lavoro.

## Aperto / bloccato

- acceptance visiva e touch sul RPi5 reale;
- input fisico Emergency e relativo debounce/health;
- adapter audio e politica di priorità con stall/terrain warnings;
- inoltro autenticato con ricevuta di persistenza alla Blackbox centrale;
- snapshot completo e replay esatto dell'esperienza pilota;
- AGL/terrain/obstacle/route clearance e vento con provenance end-to-end;
- dati pista e compatibilità con profilo approvato;
- POH/AFM dell'esemplare, OM-912 applicabile, polar e best glide approvate;
- procedure operative per tutte le otto famiglie;
- human factors, fault injection, endurance, power-cycle e HIL;
- Stabilize Assist e Autoland, che restano fuori scope e bloccati.

## Punto di ripresa

1. Rivedere la cattura sul pannello RPi5 e correggere la grafica senza cambiare
   i gate del core.
2. Collegare un ingresso fisico monitorato, ancora senza autorità di volo.
3. Definire Emergency Event Transport PFD → Blackbox con ack di persistenza.
4. Integrare AGL e terrain corridor validati; mantenere `NOT AVAILABLE` fino a
   prova end-to-end.
5. Acquisire i documenti controllati prima di popolare qualunque istruzione.
