# EFIS NEXT — audit fonti POH e procedure di emergenza

Stato: **SOURCE INTAKE / NON APPROVATO PER USO OPERATIVO**

Data: 2026-09-09

Aeromobile dichiarato: Asso V Jolly, configurazione proprietario assimilata al
Pioneer 300, motore Rotax 912 UL 80 hp.

## 1. Risultato

Le fonti pubbliche consentono di preparare lo schema dati e l'elenco delle
procedure, ma non consentono ancora di creare una checklist operativa per
l'esemplare. `PAN-002` resta bloccato e il profilo attivo resta
`SIMULATION / INDICATIVE_UNVERIFIED`.

La dichiarazione di similarità con il Pioneer 300 non è sufficiente per
trasferire limiti, velocità o procedure. Le varianti reperite differiscono per
motore, ala, massa massima, elica, carrello, flap, serbatoi e impianto
elettrico.

## 2. Gerarchia delle fonti

Ordine di autorità adottato:

1. manuale di impiego/POH e relativi supplementi appartenenti all'esemplare;
2. documenti di costruzione, collaudo, pesata e configurazione del medesimo
   esemplare;
3. manuale Rotax relativo al numero di serie del motore installato;
4. manuale e configurazione dell'elica installata;
5. documenti correnti del costruttore o titolare del progetto applicabili alla
   variante esatta;
6. manuali pubblici di altra variante, solo come riferimento;
7. brochure, rivenditori, cataloghi legacy e fonti comunitarie, mai come fonte
   di un limite operativo.

Una fonte di livello inferiore non sovrascrive una fonte superiore. Ogni dato
usato da Emergency Assist deve conservare documento, revisione, pagina,
applicabilità e stato di approvazione.

## 3. Fonti individuate

### 3.1 Asso V Jolly dell'esemplare — richiesta

Non ancora acquisiti:

- manuale di impiego/volo consegnato con l'aereo;
- pagina di identificazione, marche e numero di costruzione;
- elenco revisioni e pagine effettive;
- rapporto di pesata e limiti di centraggio aggiornati;
- scheda di collaudo e configurazione autorizzata;
- supplementi per motore, elica, carrello, flap, paracadute e autopilota;
- checklist attualmente usata dal pilota.

Questi documenti sono il gate principale. Devono essere scansionati integralmente
e conservati nella libreria tecnica controllata, non copiati nel repository
pubblico se licenza o dati personali non lo consentono.

### 3.2 Manuale Asso V pubblico — riferimento non controllato

È reperibile una copia di un `ASSO V Manuale d'impiego` di 26 pagine su un sito
terzo. La copia:

- non dimostra revisione, provenienza o applicabilità all'esemplare;
- contiene numerosi campi vuoti per motore, regimi, temperature e prestazioni;
- descrive in un punto un Rotax 912 ULS da 100 hp;
- riporta massa massima e configurazioni che possono appartenere a una vecchia
  variante;
- contiene sezioni normali, emergenza e pesata utili soltanto come indice di
  acquisizione.

Decisione: `REFERENCE_UNCONTROLLED`. Nessuna istruzione o valore sarà importato
nel motore checklist.

### 3.3 Pioneer 300

La pagina ufficiale Alpi Aviation corrente descrive il Pioneer 300 con Rotax
912 ULS, massa massima fino a 600 kg, flap elettrici, carrello retrattile e
serbatoi 2x42 l. È una configurazione diversa dal 912 UL dichiarato.

È stata inoltre individuata una copia pubblica del POH Pioneer 300
`PD9096-5`, P300 KITE, ultima revisione dichiarata 1 giugno 2015. La copia è
esterna all'area ufficiale del costruttore e non è stata associata al numero di
costruzione dell'aereo.

Alpi dispone di una Customer Area riservata ai proprietari per Service
Information e documentazione aggiornata. Questa è la strada corretta per
ottenere manuali correnti della variante Pioneer, ma non sostituisce il manuale
Asso dell'esemplare.

Decisione:

- pagina costruttore: `OFFICIAL_REFERENCE_CURRENT`;
- POH PD9096-5 pubblico: `REFERENCE_UNCONTROLLED`;
- documentazione Customer Area con applicabilità confermata:
  `CANDIDATE_CONTROLLED`, non ancora acquisita.

### 3.4 Rotax 912 UL

La pagina ufficiale Rotax conferma 912 UL da 80 hp, 59,6 kW e massimo 5800 rpm.
Rotax indica la propria Technical Documentation Library come fonte per manuali,
Service Bulletin e Service Letter e specifica che le unità SI sono autorevoli.

Il TCDS EASA E.121 è utile per identificare la famiglia documentale
`OM-912`, ma non deve essere trattato da solo come manuale dell'installazione
912 UL non certificata. Occorrono:

- fotografia targhetta e numero di serie del motore;
- Operators Manual `OM-912` nella revisione applicabile;
- Service Bulletin/Instruction applicabili a quel numero di serie;
- manuale installazione e documenti dell'installatore quando necessari;
- configurazione riduttore, elica, pompe e sensori realmente installati.

Decisione: il modello motore è identificato; limiti e procedure restano
`DOCUMENTED_CANDIDATE` finché revisione e applicabilità non sono congelate.

## 4. Audit del profilo EFIS attivo

File: `config/active/aircraft-profile.json`.

| Campo | Profilo attuale | Riscontro | Decisione |
|---|---:|---|---|
| Stato profilo | `SIMULATION` | coerente con evidenza disponibile | mantenere |
| Motore | 912 UL, 80 hp | confermato solo come modello dichiarato; manca S/N | non promuovere |
| MTOW | 600 kg | dipende dall'esemplare e dalla sua autorizzazione | verificare |
| Apertura alare | 9,4 m | fonti pubbliche Asso/Pioneer mostrano valori diversi | probabile errore |
| Lunghezza | 6,55 m | fonti pubbliche mostrano valori diversi | verificare |
| Superficie alare | 10,0 m² | fonti pubbliche mostrano anche 11 m² | verificare |
| Carburante | 2x40 l | compatibile con alcune vecchie varianti, non con tutte | verificare fisicamente |
| Flap | 0/10/15/20/30° | non coincide con il manuale Asso pubblico 12/20/28° | non usare operativamente |
| Velocità V | valori legacy in kt | nessuna fonte controllata dell'esemplare | solo simulazione |
| Best glide | 65 kt | non documentata per esemplare, peso ed elica | vietata a Emergency Assist |
| CHT max | 230 °C | valore anomalo e non supportato dalle fonti controllate | non usare / correggere dopo OM |
| Pressione olio | 25–100 psi | non distingue regime, avviamento e range normale | rifare da manuale applicabile |
| Pressione carburante | 2,2–5,8 psi | non ancora legata alla revisione OM-912 | rifare da manuale applicabile |

Non sono state cambiate le cifre del profilo: sostituire un valore indicativo
con un altro valore non applicabile produrrebbe solo un errore più credibile.

## 5. Pacchetto documentale richiesto al proprietario

Per ogni documento servono scansioni leggibili, compresi frontespizio, revisioni
e pagine bianche/compilate pertinenti:

1. manuale Asso V Jolly dell'aereo;
2. attestazione/marche e numero di costruzione;
3. rapporto di pesata e centraggio corrente;
4. pagina limitazioni e archi anemometrici;
5. procedure normali e di emergenza;
6. prestazioni, best glide e correzioni per peso/configurazione;
7. schema serbatoi, rubinetti, pompe e quantità non utilizzabile;
8. schema elettrico e protezioni;
9. carrello: logica, indicazioni e procedura estensione emergenza;
10. flap: posizioni reali, limiti e procedura guasto;
11. targhetta e numero di serie Rotax 912 UL;
12. manuale OM-912 posseduto e documenti applicabili al motore;
13. marca/modello/numero di serie elica e relativo manuale;
14. documenti paracadute balistico, se installato;
15. documenti e installazione dei due servi/autopilota.

## 6. Processo di approvazione dati

```text
RECEIVED
  -> HASHED
  -> REVISION_IDENTIFIED
  -> APPLICABILITY_CHECKED
  -> FIELD_EXTRACTED_WITH_PAGE
  -> OWNER_REVIEWED
  -> APPROVED_FOR_PROFILE
  -> RELEASED_TO_EMERGENCY_ASSIST
```

Ogni passaggio è reversibile e registrato. Una nuova revisione non sostituisce
automaticamente quella attiva: produce un diff, richiede approvazione e mantiene
rollback.

## 7. Record minimo di una procedura

Ogni procedura futura dovrà contenere almeno:

- ID stabile e tipo emergenza;
- aeromobile/configurazione applicabile;
- documento, revisione e pagina;
- condizioni di attivazione e condizioni che la inibiscono;
- memory items distinti dalle azioni di checklist;
- ordine, testo breve, conferma richiesta e possibilità di skip;
- dati dinamici ammessi e loro regole di validità;
- avvertenze e passaggio al controllo manuale;
- lingua, revisore, data di approvazione e hash del pacchetto;
- scenari di test e risultato atteso.

## 8. Prossimo gate

Il punto 3 potrà passare da acquisizione preliminare a documentazione
controllata quando il proprietario fornirà almeno:

1. frontespizio e pagina revisioni del manuale Asso dell'esemplare;
2. pagine limitazioni, velocità e procedure di emergenza;
3. targhetta con seriale del Rotax 912 UL;
4. identificazione completa dell'elica e della configurazione carrello/flap.

Fino ad allora non viene generata alcuna checklist utilizzabile in volo.

## 9. Link delle fonti censite

- Alpi Aviation Pioneer 300:
  https://www.alpiaviation.com/en/airplane/pioneer-300/
- Alpi Aviation Customer Area:
  https://customerarea.alpiaviation.com/en/new-account
- Rotax 912 UL/A/F:
  https://www.flyrotax.com/products/912-ul-a-f
- Rotax Technical Documentation:
  https://www.flyrotax.com/p/service/technical-documentation
- EASA/Rotax TCDS E.121:
  https://www.flyrotax.com/assets/files/TC-TCDS-EASA-E-121-912-915-916.pdf
- Pioneer 300 POH PD9096-5, copia terza non controllata:
  https://www.flugwelt.eu/images/Handbuecher/P300_Pilot_Operating_Handbook_1.pdf
- Asso V Manuale d'impiego, copia terza non controllata:
  https://it.scribd.com/document/522145774/manuale-di-impiego
