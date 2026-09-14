# EFIS NEXT — Voice Core Vosk replay sulla Blackbox

Stato: **SOFTWARE DEVELOPMENT / REPLAY SIM-ONLY / NON INSTALLATO**

Data: 2026-09-14

## Perimetro dell'incremento

`tools/voice/vosk_replay_adapter.py` decodifica esclusivamente un file WAV
locale PCM16 mono a 16 kHz. Richiede sia `--simulate` sia `--ptt-active` e
produce JSONL sullo standard output. Non apre microfoni, socket, seriali, CAN,
radio o collegamenti verso Pixhawk. Non conserva l'audio e non installa un
servizio.

L'output è un **candidato**, non un comando. Contiene sempre:

- `candidate_only=true`;
- `accepted=false` e `intent=NONE`;
- `transmission_or_actuation_allowed=false`;
- `simulated=true`.

Il valore `confidence` è il minimo delle confidenze delle singole parole. Se
Vosk non fornisce i dettagli parola-per-parola vale zero. Soltanto il
`VoiceCommandGate` C++ può applicare grammatica, soglia, health e PTT e
produrre un intento consultivo dell'interfaccia.

## Dipendenze future sul nodo di sviluppo

Non eseguire questi passi sul nodo operativo finché non viene autorizzata la
prova Blackbox. In una directory di staging separata usare un ambiente Python
dedicato, quindi verificare che esista una distribuzione Vosk compatibile con
Debian 13 ARM64:

```bash
python3 -m venv /opt/efis-next/voice-venv
/opt/efis-next/voice-venv/bin/python -m pip install --upgrade pip
/opt/efis-next/voice-venv/bin/python -m pip install vosk
```

Il modello deve essere scaricato da una fonte Vosk verificata, inventariato
con nome, lingua, versione, dimensione e SHA-256, poi collocato in una
directory read-only, ad esempio `/opt/efis-next/models/vosk/it/`. Il repository
non include né scarica automaticamente modelli.

## Prova replay proposta

Preparare un campione non operativo e senza dati personali, PCM16 mono 16 kHz.
Eseguire dalla radice del repository:

```bash
/opt/efis-next/voice-venv/bin/python tools/voice/vosk_replay_adapter.py \
  --simulate \
  --ptt-active \
  --language it-IT \
  --model /opt/efis-next/models/vosk/it/MODELLO_VERSIONATO \
  --wav /var/tmp/efis-voice-replay/confermo.wav
```

L'output va inizialmente conservato in una directory temporanea, non nello
spool operativo. Prima di ogni integrazione con il core devono essere
verificati schema, hash grammatica, sequenza, flag SIM e assenza di autorità.

## Gate prima del servizio Blackbox

1. compatibilità Vosk e modello su ARM64 verificata;
2. budget CPU, RAM e temperatura misurato insieme a broker e recorder;
3. corpus replay IT/EN con frasi valide, fuori grammatica e silenzio;
4. false accept e false reject misurati, senza promuovere la soglia 0,82;
5. crash, modello mancante/corrotto e risultato JSON malformato fail-closed;
6. protocollo autenticato e anti-replay definito prima della rete;
7. nessun microfono live prima di interfaccia isolata e PTT fisico qualificati;
8. touch e procedure cartacee sempre disponibili durante ogni fault.

Nessun elemento di questo runbook autorizza il deployment operativo o una
trasmissione radio automatica.
