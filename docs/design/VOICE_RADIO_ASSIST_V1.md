# EFIS NEXT — Voice Command e Radio Phraseology Assist V1

Stato: **SOFTWARE V1 PARZIALE / SIMULATION-ONLY / REPLAY VOSK NON INSTALLATO**

Data: 2026-09-14

Profilo: Professional; il solo acknowledgement di checklist può diventare una
capability opzionale del profilo Standard.

## 1. Obiettivo

Ridurre il carico cognitivo senza trasformare un recognizer probabilistico in
un controllore avionico. Il modulo permette al pilota di confermare o ripetere
elementi già presentati e mostra una bozza di comunicazione coerente con fase
di volo e dati disponibili.

La fraseologia standardizzata è la scelta primaria; il plain language resta
necessario quando la fraseologia non copre la situazione. Riferimenti correnti:

- EASA Easy Access Rules for SERA, revisione agosto 2025:
  https://www.easa.europa.eu/en/document-library/easy-access-rules/easy-access-rules-standardised-european-rules-air
- ENAC Regolamento Regole dell'Aria Italia, Ed. 4 Rev. 4:
  https://www.enac.gov.it/la-normativa/normativa-enac/regolamento-regole-dellaria-italia-ed-4-rev-4/
- ENAV AIP Italia Management and Delivery:
  https://www.enav.it/en/what-we-do/we-create-solutions-for-international-markets/informazioni-aeronautiche/aip-italia

La circolare ENAC ATM-08B è abrogata e non viene usata come fonte corrente.
ICAO Doc 9432 potrà essere aggiunto solo dopo acquisizione e verifica dei
diritti della copia ufficiale.

## 2. Architettura

```text
headset/interfono -> interfaccia audio isolata -> PTT -> Voice Core
                                                        |
navdata + flight plan + stato validato -> Phraseology Engine
                                                        |
                                      eventi/bozze versionati
                                                        |
                                      Ethernet avionica / broker
                                                        |
                                           PFD Radio Assist
                                                        |
                                    EmergencyAudioArbiter -> audio adapter
```

La freccia termina nell'interfaccia. Non esiste una freccia di ritorno verso
Pixhawk, radio o attuatori.

## 3. Grammatica V1

La grammatica è chiusa e bilingue. Gli intenti iniziali sono acknowledgement,
done, repeat, next, skip, unable, cancel, apertura/chiusura fraseologia,
apertura Emergency Assist, selezione lingua e mute/unmute non critico.

Il core normalizza maiuscole, punteggiatura e spazi, quindi richiede una
corrispondenza esatta. Non interpreta sinonimi non dichiarati. La soglia di
confidenza è configurabile ma il valore operativo dovrà derivare da prove con
microfono, accento e rumore reali; il default software non è un dato validato.

## 4. Riconoscitori

- candidato primario: Vosk offline con vocabolario ristretto;
- candidato secondario: whisper.cpp in shadow mode per confronto e
  trascrizione non operativa;
- nessun servizio cloud nel percorso primario;
- nessun output libero di un modello generativo può produrre un intento.

Il repository contiene ora un adapter Vosk **replay-only** per file WAV PCM16
mono 16 kHz e una grammatica di decoder ristretta IT/EN. L'adapter richiede
flag SIM e PTT espliciti, non apre il microfono e produce soltanto candidati
con `accepted=false`, `intent=NONE` e autorità nulla. Vosk e i modelli non sono
dipendenze installate; whisper.cpp resta soltanto un candidato futuro.

## 5. Modello fraseologico

Ogni template contiene almeno:

- ID e lingua;
- giurisdizione e tipo di servizio;
- fase di volo e direzione pilot/ATS;
- revisione, fonte, checksum e periodo di efficacia;
- campi richiesti e campi soggetti a read-back;
- stato `SIMULATION`, `REVIEWED` o `APPROVED`;
- testo controllato con placeholder espliciti.

Il motore blocca pacchetti reali non approvati, metadati incompleti, contesto
mancante e placeholder residui. Un pacchetto SIM produce soltanto
`SIMULATION_DRAFT`. Un pacchetto approvato produce
`READY_FOR_PILOT_REVIEW`, mai un'autorizzazione alla trasmissione.

## 6. Interfaccia 1024x600

La pagina `RadioAssistView` mostra:

- stazione, frequenza, fonte e validità;
- contesto di volo;
- trascrizione riconosciuta e confidenza;
- valori di read-back separati;
- bozza italiana o inglese;
- stato PTT e conferma pilota;
- banner permanente consultivo/no-transmit.

La preview usa esclusivamente dati sintetici marcati SIM. Non rappresenta una
frequenza, autorizzazione o procedura utilizzabile in volo.

## 7. Privacy e audit

Default: nessuna conservazione dell'audio. Il buffer di acquisizione è
temporaneo. L'audit registra soltanto timestamp, lingua, recognizer/versione,
hash grammatica, trascrizione normalizzata, confidenza, decisione, origine
SIM e acknowledgement risultante.

Un'eventuale registrazione radio per debriefing sarà una capability separata,
opt-in, con retention e informativa definite; non viene abilitata implicitamente.

## 8. Stati degradati

| Condizione | Comportamento |
|---|---|
| Voice disabilitato | `VOICE OFF`; touch sempre disponibile |
| PTT non attivo | `STANDBY`; nessun intento |
| recognizer guasto | `UNAVAILABLE`; nessun intento |
| confidenza insufficiente | `UNCERTAIN`; mostra candidato, nessuna azione |
| frase fuori grammatica | `REJECTED`; nessuna azione |
| fonte AIP/nav stale | bozza bloccata o campi ambra; niente valori inventati |
| rete/Blackbox persa | Voice Assist non disponibile; PFD e touch continuano |
| audio output perso | cue visuale; nessuna dichiarazione di audio riprodotto |

## 9. Verifica richiesta

1. test unitari core e parser template;
2. simulatore con frasi corrette, ambigue e fuori grammatica;
3. test microfono/interfono a motore spento;
4. rumore ventilatore, radio, motore e vibrazione;
5. matrice accenti/lingue e misure false accept/false reject;
6. perdita PTT, audio device, processo Voice Core e rete;
7. carico simultaneo recorder, MAVLink, CAN, DDP/EDP e meteo;
8. leggibilità e touch sul PFD RPi5;
9. revisione fraseologica con istruttore/operatore competente;
10. nessuna abilitazione in volo prima dei gate documentati.

## 10. Stato dell'incremento

Implementati: gate deterministico, grammatica chiusa IT/EN, separazione fra
acknowledgement e verifica sensore, compositore fail-closed, test software,
pagina grafica SIM e adapter Vosk replay-only con test deterministici tramite
recognizer fittizio.

Non implementati: dipendenza/modello Vosk su Blackbox, riconoscimento validato
su audio reale, acquisizione microfono, Whisper, PTT fisico, adapter audio,
TTS, protocollo di rete, database fraseologico operativo, integrazione
controller/QML con eventi reali, audit Blackbox e qualsiasi trasmissione radio.
