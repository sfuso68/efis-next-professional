# Alimentazione MFD/Blackbox: controllo passivo 2026-10-19

Sette campioni ogni cinque secondi per nodo, carico dei servizi già esistenti,
nessun stress test o modifica hardware/configurazione. File JSON contengono
boot ID, uptime monotono, EXT5V_V PMIC e bit throttled. Nessun reboot eseguito
dall'agente in questo controllo; entrambi i nodi risultano in boot nuovi
rispetto alle osservazioni precedenti. I timestamp iniziali non erano ancora
allineati: per gli eventi si usa il tempo monotono del boot, non la data luglio
mostrata inizialmente dal kernel Blackbox. NTP successivamente sincronizzato.

MFD: 7/7 campioni 0x0, EXT5V 5.02902–5.18044 V, nessun evento power del boot.
Blackbox: 7/7 0x50000 (bit attuali bassi zero), EXT5V 4.95666–5.04242 V;
kernel conferma undervoltage a +5.732 s e +11.780 s, normalizzati a +7.748 s e
+13.796 s nel boot corrente. Nessun nuovo evento nel campionamento.
Non è risolto il transitorio all'avvio, né identificata la causa.

La tensione core di circa 0.9 V NON è la tensione di ingresso: si usa EXT5V_V.
In entrambi i device tree max_current è 0x384 (900) e PDO USB-PD tutti zero:
questi dati non attestano una negoziazione 5 A e non sono una misura della
corrente assorbita o della capacità reale dell'alimentatore. Identificazione
fisica PSU/cavi e prova sotto carico restano necessarie; nessun override.

Riferimento ufficiale PMIC/USB-PD:
https://www.raspberrypi.com/documentation/hardware/raspberrypi/power.html

MFD attivatore candidato separatamente testato 23/23 ARM64 PASS, Windows
22 PASS/1 SKIP; pin reali release/config/environment PASS, PID 1328 invariato.
CLI check-only exit 1: qualified power approval missing (blocco atteso).
Non eseguiti attivazione, installazione drop-in o rollback reale MFD.
