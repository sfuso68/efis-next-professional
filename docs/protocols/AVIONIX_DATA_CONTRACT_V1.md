# AVIONIX NEXT — Data Contract V1

Stato: **congelato per integrazione di banco, non approvato per il volo**.
Autorità macchina: `config/protocol/avionix-data-contract-v1.json`.

## Scopo

Questo documento unifica i vincoli comuni di DDP V2 ed EDP V1 senza sostituire
le rispettive specifiche wire. Il contratto impedisce che porte, destinatari,
versioni e politiche di freshness divergano tra codice, configurazione di rete
e documentazione.

| Contratto | Dominio | Wire | UDP | Consumer | Campioni | Freshness |
|---|---|---:|---:|---|---:|---|
| DDP V2 | volo/PFD-MFD | `EFN2`, schema 2, envelope 44 B | 49002 | `pfd-01`, `mfd-01` | 23 | LIVE ≤250 ms; STALE ≤1000 ms; poi LOST |
| EDP V1 | motore/EMS | `EDP1`, schema 1, envelope 42 B | 49003 | `ems-01` | 18 | LIVE ≤250 ms; STALE ≤1000 ms; poi LOST |

DDP contiene 21 campioni numerici e due booleani. EDP contiene i 18 campioni
motore obbligatori. L'assenza, duplicazione o malformazione di un campione
obbligatorio invalida l'intero datagramma.

## Invarianti comuni

1. La Blackbox logica `47` è publisher; l'identità installata deve essere
   verificata dal consumer.
2. Il trasporto è IPv4 UDP unicast, unidirezionale, sul segmento
   `FLIGHT-DATA`. Il consumer non invia ACK o comandi.
3. Il payload contiene esclusivamente stato normalizzato e arbitrato. PFD, MFD
   ed EMS non scelgono la sorgente e non accedono all'hardware di acquisizione.
4. Ogni avvio del publisher usa una nuova `publisher_instance`; la sequenza è
   crescente nell'istanza.
5. Freshness e stati `WAITING/LIVE/STALE/LOST` dipendono dal clock monotono del
   consumer, non dal confronto tra clock di computer diversi.
6. CRC32C, schema, lunghezza, identità, duplicati e riordino sono verificati
   prima di presentare i dati.
7. SIM, degraded, stale e invalid restano espliciti. Un fallback simulato non
   può apparire come misura reale.
8. CRC e identità logica rilevano errori accidentali ma non autenticano il
   mittente. Il gate prodotto richiede isolamento fisico verificato o un
   meccanismo crittografico separato.

## Confini

- DDP non trasporta lo snapshot motore.
- EDP non trasporta dati di volo, MAVLink o CAN grezzo.
- Nessuno dei due protocolli autorizza attuazioni o configurazioni remote.
- La WLAN e la LAN di servizio non fanno parte del data plane operativo.

## Verifica offline

Eseguire dalla root del repository:

```bash
python tools/protocol/validate_data_contract.py
```

Il controllo confronta il manifesto con header C++, piano di rete e specifiche
wire. Un esito PASS è evidenza software, non verifica hardware.
