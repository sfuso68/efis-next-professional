# AVIONIX G1 — acquisizione visuale controllata

Data: 2026-09-19

Stato: **IMPLEMENTED-SW / AWAITING-EVIDENCE / NO-GO**

## Scopo

Questo passaggio acquisisce esclusivamente fotografie e trascrizioni visuali
delle quattro schede Sequent e dell'etichetta dell'alimentatore. Non effettua
scansioni I²C, letture vendor, misure elettriche, cambi DIP, modifiche al
cablaggio o attivazioni di uscite.

## Fotografie richieste

Collocare in una directory temporanea esattamente questi nove file:

1. `g1-slot-01-front.jpg` e `g1-slot-01-back.jpg` — SM-I-018;
2. `g1-slot-02-front.jpg` e `g1-slot-02-back.jpg` — prima SM-I-011;
3. `g1-slot-03-front.jpg` e `g1-slot-03-back.jpg` — seconda SM-I-011;
4. `g1-slot-04-front.jpg` e `g1-slot-04-back.jpg` — SM-I-019;
5. `g1-power-label.jpg` — etichetta completa dell'alimentatore.

Le immagini devono rendere leggibili modello, revisione, seriale se presente,
posizione visuale dei DIP e nomi dei connettori. I due moduli SM-I-011 devono
restare distinguibili come slot 02 e 03.

## Metadati

Compilare una copia di
`config/hardware/avionix-g1-visual-intake-v1.json`. Sono obbligatori operatore,
UTC, revisione, stato visuale DIP, almeno un'etichetta connettore per scheda,
modello/tensione/corrente dell'alimentatore e conferma della sorgente singola.
La dichiarazione di cattura non deve essere modificata.

## Anteprima e applicazione

L'anteprima non scrive nel repository:

```text
python tools/hardware/import_g1_visual_inventory.py \
  --source /percorso/alle/foto \
  --intake /percorso/intake-compilato.json
```

Solo dopo esame dell'anteprima si può aggiungere `--apply`. L'importatore:

- verifica nomi confinati e contenuto JPEG/PNG;
- calcola SHA-256 dei nove artefatti;
- rifiuta file mancanti, metadati incompleti e modelli discordanti;
- non sovrascrive un artefatto differente già presente;
- rifiuta la reimportazione sopra un manifest già acquisito;
- popola il manifest fisico con `VERIFIED_VISUAL`;
- mantiene `flight_approval=false` e imposta
  `NO_GO_PENDING_ENGINEERING_REVIEW`.

## Limite

L'import visuale non dimostra indirizzi I²C, canali, precisione, soglie o
calibrazione. Tali attività restano bloccate fino alla revisione tecnica del
materiale acquisito e a una successiva autorizzazione esplicita.
