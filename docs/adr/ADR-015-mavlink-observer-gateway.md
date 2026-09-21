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
- rifiuta qualsiasi destinazione nella sottorete protetta derivata dal bind
  MAVLink runtime e dal prefisso configurato (`/24` nel piano V1), incluso il
  Pixhawk, e qualsiasi destinazione uguale alla sorgente MAVLink configurata;
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

Il vincolo e' applicato nel costruttore del fan-out prima di creare il sender,
non soltanto nella configurazione o nel firewall. La rete non e' codificata
come indirizzo costante nel fan-out: il broker passa il proprio bind runtime e
il prefisso V1, dai quali viene calcolata la sottorete protetta. Un endpoint
vietato arresta l'avvio del broker in modo fail-closed. Anche il forwarder USB
provvisorio richiede un CIDR protetto esplicito a runtime e applica la stessa
esclusione a ogni destinazione, inclusi gli endpoint passati direttamente alla
classe senza attraversare il parser CLI.

Sul portatile una tabella nftables dedicata costituisce difesa in profondità:
ammette su UDP 14550 soltanto la sorgente Blackbox, scarta UDP in uscita verso
la Blackbox e vieta ogni destinazione `10.77.10.0/24`. La tabella non è la
garanzia primaria di non ritorno — che resta l'assenza del percorso nella
Blackbox — e la sua mancata attivazione mantiene l'acceptance in stato
`PARTIAL`.

QGroundControl viene avviato soltanto dopo un `ExecCondition` fail-closed che
verifica assenza USB/bridge, unicità della porta, identità SHA-256
dell'AppImage e digest completo deterministico dell'AppDir estratta. Un errore
di condizione salta l'avvio senza restart-loop.

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

Il soak di banco da dieci minuti del 2026-09-15 copre rate, monotonia, parità
fan-out/display, CPU/RSS, memoria, temperatura e throttling ed è `PASS`. Non
copre ancora perdita/jitter introdotti, endurance termica o fault WLAN reale.
Una seconda analisi retrospettiva di 60 minuti è `PASS` con copertura 97,39%,
parità esatta, zero errori/restart e throttling zero. Il reboot server è
`PASS`: nuovo boot ID, sink e firewall persistenti, health LIVE/RX-only,
listener, 12 container, Caddy protetto e zero restart. Il reboot/login della
sessione grafica resta armato e richiede ancora l'azione dell'operatore.

Il reboot/login della Ground Station e il firewall host sono ora `PASS`. La
regola di ingresso ha bloccato esattamente i 25 datagrammi di prova inviati da
una sorgente diversa dalla Blackbox; la regola di uscita ha bloccato 445
datagrammi diretti dal laptop alla Blackbox, confermando che QGC tenta traffico
di ritorno e che il filtro lo interrompe. QGC, listener e sink server sono
rimasti attivi con zero restart. Il timer di rollback è stato annullato solo
dopo la verifica.

Il fault WLAN controllato da 45 s è ora `PASS` per isolamento e recupero del
laptop: QGC ha conservato PID 2660 e `NRestarts=0`, mentre un collector
Blackbox di 180 s ha misurato 26,156 Hz MAVLink, parità observer esatta, zero
errori/restart e throttling zero. Il sink server è rimasto LIVE/RX-only. I
contatori observer provano continuità degli invii e non ricezione QGC durante
la perdita; i timestamp NetworkManager non sono disponibili, quindi il tempo
di riconnessione non è qualificato da questa prova.

Il lotto 50E aggiunge un secondo soak retrospettivo di 3.600 secondi `PASS`:
3.508 campioni, copertura 97,44%, 26,293 Hz, parità esatta e zero
errori/restart. La matrice endpoint conferma FLIGHT-DATA senza perdita e sotto
0,21 ms medi, ma rileva sulla Ground Station Wi-Fi finestre variabili dallo 0%
al 20% di perdita ICMP e RTT mdev 89–100 ms. Il socket QGC mostra coda/drop zero e
zero incremento errori UDP: ICMP non viene quindi promosso a prova di perdita
MAVLink. Il Wi-Fi resta da ottimizzare o sostituire con Ethernet per il ruolo
operativo; la fault injection controllata di perdita/jitter resta aperta.

Disabilitato il power save sul profilo e sul runtime Ground Station, la matrice
è passata: 20/20 con RTT medio 7,528 ms/mdev 4,401 ms, quindi 200/200 con
10,352 ms/mdev 17,390 ms e zero perdita. QGC ha conservato PID 2660, zero
restart e zero drop socket. Il picco 125,880 ms e l'assenza di metriche di
sequenza lato QGC mantengono Ethernet come preferenza operativa e impediscono
di qualificare il Wi-Fi come canale deterministico. La persistenza dopo reboot
resta da provare.

Il lotto 50F adotta per l'acceptance lato Ground Station una cattura passiva
Linux `AF_PACKET`, separata dal socket UDP di QGC. La cattura non trasmette e
non persiste payload; filtra soltanto il flusso Blackbox→Ground Station e
produce rate, gap, duplicati, riordino e interarrival aggregati. L'esecuzione
hardware richiede `CAP_NET_RAW`/root e resta un'azione esplicita di banco. Il
parser è strutturale, non sostituisce CRC/dialect Blackbox e non autorizza un
percorso di ritorno.

La prima cattura reale 50F conferma l'integrità per 60,091 s: 1.578 frame a
26,260 Hz, zero gap, duplicati, riordino, malformed ed errori. Non qualifica
però il timing: il massimo interarrival è 1.012,999 ms e il mdev 176,557 ms.
Di conseguenza un gate timing esplicito e una ripetizione con delta Blackbox
sincronizzato sono obbligatori prima di promuovere il collegamento Wi-Fi.

Il lotto 50G rende tale decisione eseguibile: la cattura fallisce senza
campioni timing, con massimo interarrival oltre 1.000 ms o p95 oltre 100 ms.
I limiti sono registrati nel report, derivano dal confine di salute CORE e
restano soglie di banco configurabili, non requisiti certificativi impliciti.

La prova hardware 50G fallisce correttamente il gate massimo: 4.734 frame in
180,024 s, zero difetti di sequenza o socket, ma massimo 1.032,429 ms. Il p95
di 3,120 ms e la media di 37,843 ms sono compatibili con una sorgente a
raffiche. Non è autorizzato allargare il limite: prima vanno misurati
HEARTBEAT, periodicità delle pause e i due lati Ethernet/Wi-Fi della Blackbox.

Il lotto 50H introduce lo schema report v2: distribuzione degli intervalli,
raffiche e timing HEARTBEAT sono aggregati senza payload. HEARTBEAT usa un gate
separato di 3.000 ms coerente con `lost_after`. La decisione architetturale
resta conservativa: prima si confrontano ingresso Ethernet e uscita Wi-Fi,
poi si decide se intervenire su sorgente, broker o trasporto.

Il confronto hardware 50H mostra la stessa firma già su Ethernet in ingresso:
179 pause oltre 500 ms e 194 raffiche in 180 s, contro 180/196 sul portatile.
Entrambi i lati hanno 180 HEARTBEAT e zero difetti di sequenza; il Wi-Fi porta
il massimo HEARTBEAT da 1.000,341 a 1.046,943 ms, sempre entro 3.000 ms. La
decisione è quindi di distinguere tre esiti: integrità, liveness e uniformità.
Il gate uniforme Wi-Fi resta fallito e visibile, ma non equivale a perdita del
link monitor-only. Ethernet rimane preferita per una Ground Station operativa.

Il lotto 50I codifica la distinzione: integrità e liveness determinano
`link_status`; l'uniformità determina `quality_status`; l'esito globale resta
FAIL se uno dei tre gruppi fallisce. La condizione Wi-Fi reale diventa quindi
link PASS, quality DEGRADED, overall FAIL. Questa rappresentazione evita sia il
falso allarme di link perso sia la promozione indebita della qualità.

Il lotto 50J sceglie per la Ground Station Ethernet il trasferimento esclusivo
dell'identità `192.168.1.39` da `wlo1` a `eno1`. In tal modo fan-out e firewall
non cambiano; è vietato tenere l'indirizzo su entrambe le interfacce. Il
preflight reale è `BLOCKED` soltanto perché `eno1` è `NO-CARRIER`: nessun
profilo o cambio route è autorizzato prima del collegamento fisico e del
rollback armato da console locale.

Il lotto 50K corregge un'ambiguita' topologica del runbook 50J: `eno1` deve
entrare nella LAN di servizio `192.168.1.0/24` tramite router/switch LAN, non
nel Waveshare FLIGHT-DATA `10.77.10.0/24`. Il preflight v2 rifiuta nel
post-switch una route FLIGHT-DATA diretta su `eno1`, un percorso instradato
verso la Blackbox di servizio e `ip_forward=1`. Non sono ammessi alias di
servizio sulla porta FLIGHT-DATA della Blackbox, bridge o NAT. L'accesso SSH
della sessione di sviluppo alla Ground Station non e' disponibile; pertanto
nessuna nuova prova hardware o mutazione di rete e' dichiarata.

Fino a queste prove lo stato resta `PARTIAL` e il sistema non è approvato per
il volo.
