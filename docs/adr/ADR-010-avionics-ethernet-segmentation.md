# ADR-010 — Segmentazione Ethernet avionica

- Stato: proposta per il banco V1; approvazione hardware ancora richiesta
- Data: 2026-09-07
- Riferimenti: ADR-002, ADR-003, ADR-008, ADR-009

## Decisione

La Blackbox resta l'unico broker e punto di normalizzazione. I display non
ricevono MAVLink grezzo e non comunicano con il Pixhawk. Il cablaggio Ethernet
usa due domini distinti:

- **FLIGHT-DATA** `10.77.10.0/24`: Pixhawk, Blackbox, PFD, EMS e futuro MFD;
- **VIDEO** `10.77.20.0/24`: quattro camere e un nodo VIDEO-CORE dedicato.

Entrambe le reti operative usano IPv4 statico, MTU 1500, nessun DHCP e nessun
default gateway. Il server di casa, Internet e l'hotspot non appartengono alla
rete FLIGHT-DATA. La Blackbox può sincronizzare dati tramite una seconda
interfaccia di servizio, ma non opera come router o bridge IP: trasferisce
soltanto oggetti applicativi validati e stato normalizzato.

Il flusso primario è:

```text
Pixhawk 10.77.10.3 --MAVLink2/UDP 14550--> Blackbox 10.77.10.2
Blackbox 10.77.10.2 --DDP V2/UDP 49002--> PFD 10.77.10.11
                                           EMS 10.77.10.12
                                           MFD 10.77.10.13 (futuro)
```

Il VIDEO-CORE è un processo e un computer separato dall'EMS. Aggrega, controlla
e registra le camere; pubblica ai display solo il flusso selezionato o il
mosaico. Un carico o guasto video non deve sottrarre CPU, memoria, rete o
alimentazione al PFD, all'EMS o alla Blackbox.

## Valutazione dello switch Waveshare

La targhetta fotografata il 2026-09-07 identifica il dispositivo come Waveshare
**Gigabit-Switch-5P (EU)**. È uno switch plug-and-play non gestito: non
fornisce VLAN, ACL o QoS configurabili. È utilizzabile come switch FLIGHT-DATA
del prototipo, con cinque porte dedicate, ma non deve ospitare anche le quattro
camere. Non basta inoltre alla configurazione finale con Blackbox, Pixhawk,
PFD, EMS, MFD e VIDEO-CORE: servono almeno sei porte operative, oltre a una
porta di manutenzione desiderabile. La rete video richiede un secondo switch
separato, preferibilmente gestito e con alimentazioni camera individualmente
protette.

La verifica della targhetta chiude l'identificazione del modello, non i gate di
rete o ambientali. La dicitura commerciale «industrial grade» non costituisce una qualifica
aeronautica. Alimentazione, transitori, EMI, temperatura, vibrazioni,
connettori e ritenzione dei cavi restano gate hardware indipendenti.

Una sola VLAN su uno switch gestito non sostituisce la separazione fisica per
il prodotto: condivide alimentazione, backplane e failure mode. Nel prototipo
sono ammesse VLAN separate; per l'architettura finale si preferiscono due
switch o almeno due domini fisici indipendenti.

## Vincoli di sicurezza

- `net.ipv4.ip_forward=0` e nessun bridge fra WLAN/service e FLIGHT-DATA;
- UDP unicast, niente broadcast MAVLink e niente multicast iniziale;
- Blackbox in ricezione MAVLink; nessuna risposta, richiesta stream o comando;
- PFD/EMS accettano DDP soltanto dall'indirizzo e dal nodo Blackbox previsti;
- nessun jumbo frame finché tutti i nodi non sono collaudati: MTU 1500;
- Wi-Fi ammesso per manutenzione durante la migrazione, non come data plane di
  volo;
- modifiche `NET_*` Pixhawk solo dopo backup parametri, diff previsto,
  read-back e rollback documentato.

## Conseguenze e limiti

La Blackbox è un punto di concentrazione intenzionale e quindi un failure mode
da gestire: i display devono mostrare `STALE/LOST`, mentre Pixhawk continua il
controllo di volo indipendentemente dall'EFIS. La perdita della Blackbox non può
alterare ArduPlane. Per dichiarare professionale il sistema servono ancora
autenticazione DDP oppure isolamento fisico verificato, alimentazioni protette,
fault injection, misure di jitter/latenza e un percorso degradato definito.

Fonti hardware/protocollo:

- [ArduPilot Ethernet / Network Setup](https://ardupilot.org/plane/docs/common-network.html)
- [Waveshare Gigabit-Switch-5P](https://www.waveshare.com/product/gigabit-switch-5p.htm)
