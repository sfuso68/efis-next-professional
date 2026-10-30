# Simulatore — quadro di completamento

2026-10-30, R1-30. DEVELOPMENT / NON-CERT. Nessuna approvazione al volo.
Il piano canonico macchina è config/simulation/development-plan-v1.json;
questa vista esplicita i gap,non sostituisce Master Matrix o evidence bundle.
Non calcolare avanzamento del prodotto dal numero dei test automatici.

| Pacchetto | Dimostrato | Mancante per chiusura |
|---|---|---|
| P01 contratto | Schema,validazione e vettori negativi,confini SIM | Review semantica e mapping completo dei prodotti |
| P02 producer | JSBSim100Hz/frame20Hz,lifecycle/epoch,consumer non bloccanti,recorder test bounded | Soak,ripetibilità,power-loss e watchdog finalizzazione |
| P03 comandi | Ammissione/dedup/lease,fixture locale con tre ingressi FDM e readback/audit; inbox bounded isolato e lookup outcome dopo revoca | Collegamento inbox al loop owner/pausa,console live autenticata,missione pilotata,lifecycle/epoch,audit live e replay esecutivo |
| P04 distribuzione | Checker DDP/EDP offline e precedente link PFD ristretto | Nuovo producer verso broker e PFD/MFD/EMS simultanei;source arbitration e correlazione sessione |
| P05 impianti | Motore C172P reference,fuel/flow,flap parziale,canali mancanti null | Rete elettrica,sonde per cilindro,pressioni/carburante,airframe e guasti accoppiati |
| P06 NAV/WX | Mondo analitico offline,VOR/NDB/ILS/DME/route/GNSS e prodotti WX sintetici | Prodotti attraversano parser/cache/HMI reali,TAF dinamico,identità NAV e propagazione RF realistica |
| P07 catalogo | Hash modello/runtime,pack/restore bounded run60s su server e copia Windows con verifica semantica | Catalogo licenze/provenienza/versioni,dataset completi,replica e restore SIL NAS verificato;restore runtime/modelli |
| P08 consumer | Prove storiche del PFD con sottoinsieme dati SIM | Tre applicazioni attuali dalla stessa sessione,senza generatori concorrenti;EMS/airframe/WX completi |
| P09 terreno | SVS e cache esistenti nel prodotto | Collisione/DEM/ostacoli/datum legati alla sessione,route packs e guasti attraverso consumer reali |
| P10 funzioni | Moduli applicativi esistenti,non nuova acceptance SIL | Emergenze/voce/FPV/setup/accessori con guasti coerenti e isolamento ore operative |
| P11 modelli | Solo C172P REFERENCE_UNVALIDATED | Asso/Pioneer distinti,Rotax912UL/ULS distinti,manuali/riferimenti indipendenti e tolleranze |
| P12 acceptance | Test automatici e run brevi con confini espliciti | Missione completa,endurance,16scenari,multi-panel,HMI/touch/power/golden e restore |

## Sequenza di uscita

1. Chiudere il control-plane di TEST e la missione pilotabile: fixture primo
   secondo non è una console e non basta a dichiarare P03 completo.
2. Nuovo broker/fanout software con flight,engine e oggetti applicativi
   coerenti;prima consumer headless reali,poi pannelli con gate separati.
3. Completare impianti/sonde e prodotti NAV/WX/airframe senza riempire canali
   assenti con valori plausibili. UL e ULS non sono lo stesso modello.
4. Catalogo/restore,terreno e scenari cross-module,poi acceptance software
   ripetibile;le prove di alimentazione/HMI sul banco restano distinte.

Nessun pacchetto è dichiarato DONE. Il SIL resta NOT_COMPLETE finché i
criteri di uscita del piano non hanno evidenze specifiche. Le esecuzioni su
serverha sono software su server reale,non collaudo hardware avionico.

Gate software workstation R1-28:una suite nativa di replay non ha potuto
avviare un caso per WinError4551 (controllo applicazioni). CTest complessivo
non è verde;non aggirare la policy o equiparare i149test Python e159server
PASS a una nuova acceptance dei binari/pannelli.
