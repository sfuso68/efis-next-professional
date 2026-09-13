# EFIS NEXT — Emergency Input e Audio Policy V1

Stato: **CORE SOFTWARE IMPLEMENTED AND TESTED / ADAPTER HARDWARE APERTI**

Data: 2026-09-13

## Ingresso fisico

`EmergencyInputMonitor` è una macchina a stati C++20 indipendente dal tipo di
ingresso. Condiziona un campione `pressed/source_healthy` e produce al massimo
un fronte di attivazione per ogni pressione valida. Applica:

- debounce configurabile (default 50 ms);
- pressione prolungata configurabile (default 1,2 s);
- riarmo soltanto dopo rilascio stabile (default 300 ms);
- diagnosi e disarmo di una linea bloccata premuta (default 10 s);
- fail-closed immediato quando l'adapter dichiara la sorgente non sana.

Il monitor non contiene GPIO, evdev, CAN, MAVLink o comandi. Il pin/dispositivo,
il livello elettrico, la ridondanza e il cablaggio non sono scelti: finché non
sono definiti e provati, la funzione fisica resta non operativa.

## Priorità audio

`EmergencyAudioArbiter` accetta cue testuali classificati `ADVISORY`, `CAUTION`,
`WARNING` e `CRITICAL`. Un cue di priorità superiore interrompe quello attivo;
a parità di priorità vale FIFO. ID duplicati sono soppressi e ogni cue ha un
tempo minimo di ripetizione. Il mute non critico elimina advisory/caution, ma
non può sopprimere warning/critical.

Questa è solo la policy deterministica. Non sono ancora implementati né
verificati l'adapter ALSA/PipeWire, amplificatore, altoparlante, cuffie,
intelligibilità, livello sonoro, perdita device o mix con gli avvisi primari.
STALL e TAWS sono citati come classi di priorità, non come funzioni dichiarate
complete.

## Verifica software

`efis_emergency_io_tests` copre rimbalzo, hold, singolo fronte, riarmo,
sorgente guasta, stuck, configurazione non valida, preemption, mute e repeat
inhibit. La prova software non chiude i gate del pannello o dell'audio reale.

## Confine di sicurezza

L'uscita dell'ingresso può soltanto richiedere l'apertura del workflow
consultivo con conferma pilota previsto da ADR-012. L'audio presenta messaggi;
nessuno dei due componenti ha autorità sul controllo di volo.
