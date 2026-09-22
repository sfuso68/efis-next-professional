# AVIONIX NEXT — Synthetic Vision LOD/projection correction V1

Stato: `VERIFIED_SW / VERIFY_HW`

Attività: `AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`

Prodotto: DEVELOPMENT / NON-CERT / `flight_approval=false`

## Difetto corretto

Il renderer Quick 3D sceglieva il livello di dettaglio dalla distanza fra la
camera e il **centro** della tile. Sul confine `N45E012` → `N46E012` la camera
si trova a circa 55,66 km da entrambi i centri, oltre la soglia precedente di
52 km: le due tile passavano quindi simultaneamente al DEM a bassa densità
proprio durante la transizione congelata dal benchmark C9. Il risultato poteva
apparire come una fascia stirata o sfocata in primo piano.

## Contratto C12

- trasformazione geografica e costanti ENU locali sono in
  `efis/display/terrain_projection.hpp`, non duplicate nel QML;
- il LOD usa la distanza minima dal rettangolo geografico coperto dalla tile;
- una camera dentro la tile o sul suo bordo ha distanza zero;
- ingresso high-detail a 18 km ed uscita a 24 km dal perimetro formano una
  isteresi che impedisce oscillazioni visive vicino alla soglia;
- il pre-caricamento è limitato a 45 km dal perimetro, evitando di mantenere
  sempre attive tutte le sei mesh;
- coordinate o distanze non finite falliscono chiuse: niente high-detail e
  niente caricamento;
- il runtime scrive l'evento `terrain_projection_contract` con contratto,
  criterio LOD e soglie effettive.

Le soglie di accettazione C9 non sono state modificate: almeno 40 FPS, p95 non
oltre 25 ms, frame massimo 100 ms, `get_throttled=0x0`, continuità della
transizione, fault missing-tile esplicito e firma operatore restano vincolanti.

## Verifica software

La prova `efis_terrain_projection_tests` verifica origine, centri tile,
continuità sul confine, posizione LIPH, isteresi, pre-caricamento e input non
finiti. La configurazione MSVC/Qt 6.11.1 e la build completa, compreso
`efis-pfd-qt` e la compilazione QML AOT, sono PASS.

Le quattro suite direttamente interessate sono PASS:

1. `efis_terrain_projection_tests`;
2. `efis_synthetic_vision_benchmark_tests`;
3. `efis_synthetic_vision_session_tests`;
4. `efis_synthetic_vision_run_assembler_tests`.

Una cattura offscreen 1024×600 ha verificato soltanto l'avvio del renderer e
non costituisce evidenza di qualità Quick 3D. Il run CTest completo Windows ha
eseguito 64 suite: 54 PASS e 10 non eseguibili/fallite per Smart App Control o
caricamento DLL Qt del banco Windows già documentato. Nessuna delle quattro
suite C9–C12 è fra queste anomalie.

## Verifica hardware ancora obbligatoria

C12 non è stato distribuito su PFD o MFD. Per chiudere il punto occorre:

1. creare una sessione C10 dal commit realmente installato su entrambi i nodi;
2. percorrere i cinque checkpoint DAY/NIGHT e il confine N45/N46;
3. assemblare con C11 frame timing, termica, tile transition, missing-tile,
   dieci screenshot e firma operatore;
4. confrontare la baseline precedente e verificare che il primo piano sia
   leggibile senza introdurre seam, popping o regressioni TAWS;
5. superare integralmente C9 su PFD e MFD reali.

Fino a quel momento `AVX-G3-01` resta aperto e `SV-001` resta `PARTIAL`.
