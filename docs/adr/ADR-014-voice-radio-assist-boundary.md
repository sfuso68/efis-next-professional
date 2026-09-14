# ADR-014 — Voice Assist e Radio Assist consultivi

- Stato: accettata per Software V1
- Data: 2026-09-14
- Riferimenti: ADR-002, ADR-009, ADR-010, ADR-012, ADR-013,
  `VOICE_RADIO_ASSIST_V1.md`

## Decisione

EFIS Next separa il riconoscimento vocale in due funzioni:

1. `Voice Command Gate`, che trasforma soltanto frasi appartenenti a una
   grammatica chiusa in eventi dell'interfaccia pilota;
2. `Radio Phraseology Assist`, che compone testo consultivo da template
   versionati e da contesto esplicito.

Entrambe sono disabilitabili dal pilota, funzionano offline e non contengono
un percorso di comando verso Pixhawk, ArduPilot, servi, radio, transponder,
flap, carrello o altri attuatori. Una decisione vocale accettata può al massimo
equivalere alla pressione di un controllo UI consultivo.

La V1 richiede push-to-talk. Il riconoscimento continuo non è ammesso. Un
risultato sotto soglia, con recognizer non sano, senza PTT o fuori grammatica
non genera alcun intento.

## Conferma non equivale a verifica

Ogni elemento consultivo conserva separatamente:

- presentazione al pilota;
- acknowledgement vocale o touch;
- verifica tramite sensore;
- condizione non verificabile.

Dire `FATTO` o `CONFERMO` registra un acknowledgement, non prova comprensione
e non rende vera una condizione impiantistica.

## Fraseologia

Italiano e inglese sono template distinti, non traduzioni generate. Un
pacchetto operativo deve dichiarare ID, giurisdizione, revisione, fonte,
checksum, applicabilità e stato di approvazione. La composizione fallisce se
manca un campo richiesto o resta un token irrisolto. Anche un testo completo
resta `PILOT REVIEW REQUIRED` e `transmit_allowed=false`.

I valori dinamici operativi — frequenza, pista, QNH, quota, codice transponder,
spazio aereo e NOTAM — devono provenire da sorgenti aeronautiche correnti e
con freshness visibile. Il motore non completa valori mancanti con inferenze.

## Allocazione

La prima integrazione del recognizer è prevista sulla Blackbox RPi5, con
limiti di CPU/memoria e senza interferire con broker o recorder. Il display
RPi5 riceve soltanto intenzioni e bozze strutturate. Un guasto del servizio
voce lascia sempre disponibili touch e procedure cartacee. Prima del prodotto
va rivalutato un nodo Voice/Audio dedicato per evitare il failure mode comune.

## Alternative escluse

- modello generativo nel percorso di comando o nella generazione della
  fraseologia operativa;
- trascrizione libera usata direttamente come comando;
- ascolto sempre attivo in cabina;
- trasmissione radio automatica;
- registrazione audio permanente attiva per impostazione predefinita;
- riconoscimento vocale che avanza una checklist senza presentazione e stato
  tracciabile dell'elemento.

## Gate hardware

La V1 software non prova intelligibilità, latenza o robustezza. Servono ancora
interfaccia audio isolata, scelta del punto interfono, PTT fisico, adapter
ALSA/PipeWire, prove con rumore reale, perdita dispositivo, falsi positivi e
negativi, priorità con STALL/TAWS, reboot e carico simultaneo Blackbox.
