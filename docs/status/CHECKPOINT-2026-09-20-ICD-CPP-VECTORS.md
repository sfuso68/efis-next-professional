# CHECKPOINT — vettori ICD consumati dal C++ nativo

Data: 2026-09-20  
Branch: `safety/icd-cpp-vectors-v1`  
Gate: G2, verifica software

## Difetto riprodotto

I digest DDP V2/EDP V1 erano prodotti e ricontrollati dal validatore Python.
I test C++ verificavano codec e sessioni con casi propri, ma non consumavano il
JSON congelato. Inserito il nuovo target prima del sorgente, CMake è fallito
come previsto su `tests/avionix_icd_vector_tests.cpp` mancante.

## Correzione

Il nuovo eseguibile CTest:

- legge direttamente il JSON dei vettori congelati;
- prepara campioni canonici con identità nodo 47, istanza
  `0x0102030405060708`, sequenza 1 e tempo monotono 1.000.000 µs;
- chiama i codec C++ operativi `ddp::encode_snapshot` ed
  `edp::encode_snapshot`;
- confronta lunghezza frame, CRC32C payload e SHA-256 wire;
- usa una SHA-256 interna al test, indipendente dal generatore Python.

## Evidenza

- Build MSVC: PASS.
- `efis_avionix_icd_vector_tests`: PASS in CTest.
- Nessun file ICD o vettore congelato è stato modificato.

## Limiti

- Verifica corrente x86-64 Windows; il nuovo binario non è ancora stato
  eseguito sui tre ARM64.
- Nessuna prova di rete live e nessuna autorizzazione al volo.
