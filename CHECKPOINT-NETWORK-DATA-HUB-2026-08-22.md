# Checkpoint EFIS NEXT Professional — 22 agosto 2026

## Punto fissato

Il progetto mantiene una sola codebase e due profili, Standard e Professional.
I progetti precedenti restano separati e non vengono sostituiti.

La direzione di piattaforma concordata e':

- Ethernet come backbone comune dei nodi di bordo;
- Pixhawk 6X collegabile direttamente alla rete con MAVLink 2 su UDP/TCP;
- blackbox evoluta in **EFIS Data Hub**, collettore, normalizzatore, registratore e
  distributore dei dati aggiuntivi;
- switch Ethernet separato come centro fisico della rete, affinche' la blackbox
  non sia anche un singolo punto di guasto hardware;
- percorso diretto Pixhawk → PFD per le grandezze primarie, affiancato dal
  percorso normalizzato Pixhawk/CAN → Data Hub → display;
- CAN A/CAN B raccolti dal Data Hub e mantenuti come bus deterministici per
  sensori, EMS e moduli locali;
- rete video separata logicamente dalla telemetria, preferibilmente tramite
  VLAN dedicata;
- hotspot e server centrale attestati sul Data Hub, senza esporre direttamente
  il Pixhawk a Internet.

## Stato verificato

- branch: `work/codex-multisource-p1`;
- pipeline Pixhawk + ESP32/Matek con arbitrato per singolo segnale;
- failover IAS Pixhawk → ESP32 verificato;
- eventi di cambio sorgente registrabili con identita' della grandezza;
- build Windows: riuscita, 10/10 test;
- build ARM64 Raspberry Pi 5: riuscita, 10/10 test;
- smoke test RPi5: PFD circa 41 FPS, EMS circa 40 FPS;
- Pixhawk 6X identificato con ArduPlane 4.7.0;
- Matek/DLVR pubblica pressione differenziale e temperatura tramite ArduPilot;
- M10 presente ma senza fix nella prova al banco;
- restano calibrazione accelerometri, bussola e configurazione della rete
  Ethernet del Pixhawk.

## Decisioni rinviate deliberatamente

Non sono ancora stati modificati:

- parametri `NET_*` del Pixhawk;
- indirizzamento definitivo della rete avionica;
- configurazione dello switch e delle VLAN;
- servizio permanente EFIS Data Hub sulla blackbox;
- protocollo wire definitivo EFIS CORE V2;
- installazione attiva del PFD RPi5.

Il prossimo lavoro di rete ripartira' da questo documento, dopo backup completo
dei parametri ArduPilot e verifica del cablaggio Ethernet della baseboard 6X.

## Nuovo filone attivo

Prima di proseguire con il backbone, il lavoro passa al nodo Raspberry Pi 4
dedicato all'EMS. La specifica di riferimento e':

`docs/design/EMS_PROFESSIONAL_RPI4_V1.md`

