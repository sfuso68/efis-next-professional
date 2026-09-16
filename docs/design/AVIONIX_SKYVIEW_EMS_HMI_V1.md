# AVIONIX NEXT — SkyView e EMS HMI Contract V1

## Stato e confini

Questa specifica congela la direzione grafica delle schermate SkyView e EMS.
Il mockup approvato è conservato in
`assets/concepts/avionix-skyview-ems-hmi-v1.png` (SHA-256
`971dca214fafddba48bf66e1b1db5a0e091be34790fa6150e42783fda0320087`). È un
riferimento di composizione, non un display operativo
e non costituisce approvazione al volo. La sorgente macchina è
`config/ui/avionix-hmi-v1.json`.

RPi5 esegue PFD, MFD, SkyView e navigazione. RPi4 esegue l'EMS dedicato; il
secondo RPi4 VIDEO-CORE resta fisicamente distinto. Tutte le pagine sono
consumer read-only: non introducono autorità su Pixhawk, motore, servi o
impianto elettrico.

## Dieci gate di implementazione

### HMI-01 — Canvas e adattamento

- Canvas logico 1024×600.
- SkyView: 1024×600, 1280×800, 1920×1080 su RPi5.
- EMS: 800×480, 1024×600, 1280×800 su RPi4.
- Il layout usa zone e breakpoint; non è ammesso il semplice scaling uniforme.

### HMI-02 — Design system

Tipografia, spaziatura, stroke e colori provengono da token comuni. Target
touch minimo 44 px, testo primario minimo 34 px, secondario minimo 18 px.
Warning, caution, navigation e invalidità hanno semantica esclusiva. Il colore
non è mai l'unico indicatore di una condizione safety-related.

### HMI-03 — Provenienza e qualità dati

Ogni valore operativo conserva sorgente, timestamp, sequenza, unità, qualità e
stato `LIVE/STALE/LOST/DEGRADED/FAIL/SIM`. LIVE vale fino a 250 ms, STALE fino
a 1000 ms. SIM deve essere sempre visibile. Un ultimo valore congelato mostra
età e invalidità e perde il colore normale.

### HMI-04 — Synthetic vision

Il terreno mostra provenienza ed età; runway corridor e pista appaiono soltanto
con aeroporto/pista identificati e geometria valida. Ostacoli, meteo e traffico
sono layer distinti con health separato. Il primo piano non deve essere
sfocato, stirato o sostituito da texture decorative.

### HMI-05 — Strumenti di volo

I nastri trasparenti non toccano i bordi e non hanno cornici pesanti. IAS e ALT
rimangono leggibili senza sovrapporsi alle scale. L'anemometro espone
`VS0/VS1/VX/VY/VFE/VNO/VNE`; il valore IAS cambia semantica cromatica insieme a
testo/marker, mai con il solo colore. Heading ribbon superiore e pitch ladder
restano compatti e indipendenti dal terreno.

### HMI-06 — Navigazione

La rosa inferiore è circolare, mai ellittica, con arco visibile ±35°. DTK, DIS,
ETE, CDI, waypoint attivo e vento rimangono all'interno dell'arco senza
interferire con nastri o flight path marker. Il tocco dell'HSI apre la pagina
Navigation; nessuna azione cambia una sorgente avionica.

### HMI-07 — EMS primario

RPM e MAP dominano la pagina. Pressione olio, temperatura olio e fuel flow sono
secondari ma leggibili. Scale, archi e limiti provengono dal profilo motore
approvato; finché non è disponibile il manuale applicabile i limiti restano SIM
o UNVERIFIED. CHT/EGT mostrano quattro cilindri, valori, hottest channel e
spread senza nascondere un sensore guasto.

### HMI-08 — Airframe, fuel ed electrical

Flap verticale con detent 0/10/15/20/30°, posizione comandata e misurata.
Carrello con consensi NOSE/LEFT/RIGHT e stato non ambiguo. Fuel mostra sinistra,
destra, totale ed endurance senza overflow. L'elettrico rappresenta battery,
alternator, main/essential bus, pompa e doppia accensione, con provenienza e
health; non implica capacità di comando.

### HMI-09 — Degrado e reversione

LOST/FAIL rimuovono qualunque normalizzazione verde. Il valore precedente può
restare solo con age esplicita. SkyView senza terrain torna a PFD convenzionale;
EMS senza EDP passa a pagina fail-safe e non inventa dati locali. La banda EMS
su PFD/MFD è un consumer riepilogativo, non sostituisce il nodo EMS.

### HMI-10 — Prestazioni e acceptance

Minimo stabile 40 FPS, obiettivo 50 FPS. Golden screenshot alle risoluzioni
target, controllo overflow, font, cerchi non ellittici, contrasto, stati
LIVE/STALE/LOST/SIM e touch. La qualifica richiede poi 60 minuti su hardware,
termica, throttling, fault dati e valutazione umana diurna/notturna.

## Ordine di sviluppo

1. Token e componenti geometrici condivisi.
2. SkyView senza layer opzionali, poi terrain/obstacle/weather/traffic.
3. Gauge EMS V4, cylinder matrix e sinottici airframe/electrical.
4. Binding ai modelli DDP/EDP conservando health e provenienza.
5. Golden/replay software; solo dopo, build ARM64 e acceptance sui pannelli.

## Criterio di uscita

Il contratto passa il gate software quando manifesto e test sono verdi e tutte
le risoluzioni producono golden senza overlap. Non diventa `DONE` finché i
criteri HMI-10 non sono verificati sui pannelli reali.
