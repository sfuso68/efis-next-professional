# EFIS NEXT — Operational Parameter Register V2

> Generated from `data/catalog/operational-parameter-dictionary-v2.json`.
> Do not edit manually. Every identifier below becomes one row per materialized profile.

**Parameters:** 447  
**Domains:** 28  
**Dictionary version:** 2.0.0

## Identità aeromobile (`identity`) — 14

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `identity.manufacturer` | Costruttore | TEXT | — | REQUIRED | OPERATIONAL |
| `identity.model` | Modello | TEXT | — | REQUIRED | OPERATIONAL |
| `identity.variant` | Variante | TEXT | — | REQUIRED | OPERATIONAL |
| `identity.construction_number` | Numero di costruzione | TEXT | — | REQUIRED | OPERATIONAL |
| `identity.registration` | Marche di identificazione | TEXT | — | REQUIRED | OPERATIONAL |
| `identity.category` | Categoria operativa | ENUM | — | REQUIRED | OPERATIONAL |
| `identity.icao_type` | Codice tipo ICAO | TEXT | — | CONDITIONAL | OPERATIONAL |
| `identity.construction_year` | Anno di costruzione | INTEGER | — | REQUIRED | OPERATIONAL |
| `identity.seats` | Posti | INTEGER | — | REQUIRED | OPERATIONAL |
| `identity.build_standard` | Standard o revisione costruttiva | TEXT | — | REQUIRED | OPERATIONAL |
| `identity.configuration_id` | Identificativo configurazione | TEXT | — | REQUIRED | OPERATIONAL |
| `identity.base_airport` | Aeroporto base | TEXT | — | OPTIONAL | OPERATIONAL |
| `identity.owner_operator` | Proprietario o operatore | TEXT | — | OPTIONAL | INFORMATIONAL |
| `identity.external_markings` | Livrea e marcature esterne | TEXT | — | OPTIONAL | INFORMATIONAL |

## Configurazione regolamentare (`regulatory`) — 16

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `regulatory.state` | Stato di registrazione | TEXT | — | REQUIRED | OPERATIONAL |
| `regulatory.authority` | Autorità o ente | TEXT | — | REQUIRED | OPERATIONAL |
| `regulatory.certificate_type` | Tipo certificato o attestato | TEXT | — | REQUIRED | OPERATIONAL |
| `regulatory.certificate_number` | Numero certificato o attestato | TEXT | — | REQUIRED | OPERATIONAL |
| `regulatory.approval_basis` | Base di approvazione | TEXT | — | REQUIRED | OPERATIONAL |
| `regulatory.operating_categories` | Categorie operative ammesse | JSON | — | REQUIRED | OPERATIONAL |
| `regulatory.day_vfr_allowed` | VFR diurno ammesso | BOOLEAN | — | REQUIRED | OPERATIONAL |
| `regulatory.night_vfr_allowed` | VFR notturno ammesso | BOOLEAN | — | REQUIRED | OPERATIONAL |
| `regulatory.ifr_allowed` | IFR ammesso | BOOLEAN | — | REQUIRED | OPERATIONAL |
| `regulatory.aerobatic_allowed` | Acrobazia ammessa | BOOLEAN | — | REQUIRED | OPERATIONAL |
| `regulatory.minimum_crew` | Equipaggio minimo | INTEGER | — | REQUIRED | OPERATIONAL |
| `regulatory.maximum_occupants` | Occupanti massimi | INTEGER | — | REQUIRED | OPERATIONAL |
| `regulatory.required_documents` | Documenti obbligatori a bordo | TABLE | — | REQUIRED | OPERATIONAL |
| `regulatory.required_placards` | Targhette obbligatorie | TABLE | — | REQUIRED | OPERATIONAL |
| `regulatory.noise_certificate` | Certificato rumore | TEXT | — | CONDITIONAL | OPERATIONAL |
| `regulatory.operational_restrictions` | Restrizioni operative | TABLE | — | REQUIRED | OPERATIONAL |

## Geometria e riferimenti (`geometry`) — 16

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `geometry.length` | Lunghezza | NUMBER | m | REQUIRED | OPERATIONAL |
| `geometry.height` | Altezza | NUMBER | m | REQUIRED | OPERATIONAL |
| `geometry.wingspan` | Apertura alare | NUMBER | m | REQUIRED | OPERATIONAL |
| `geometry.wing_area` | Superficie alare | NUMBER | m2 | REQUIRED | OPERATIONAL |
| `geometry.mean_aerodynamic_chord` | Corda aerodinamica media | NUMBER | m | REQUIRED | OPERATIONAL |
| `geometry.aspect_ratio` | Allungamento alare | NUMBER | 1 | OPTIONAL | OPERATIONAL |
| `geometry.airfoil` | Profilo alare | TEXT | — | OPTIONAL | OPERATIONAL |
| `geometry.wing_incidence` | Incidenza alare | NUMBER | deg | OPTIONAL | OPERATIONAL |
| `geometry.wing_dihedral` | Diedro alare | NUMBER | deg | OPTIONAL | OPERATIONAL |
| `geometry.wheelbase` | Passo carrello | NUMBER | m | CONDITIONAL | OPERATIONAL |
| `geometry.wheel_track` | Carreggiata | NUMBER | m | CONDITIONAL | OPERATIONAL |
| `geometry.propeller_ground_clearance` | Distanza elica-suolo | NUMBER | m | CONDITIONAL | SAFETY_CRITICAL |
| `geometry.reference_datum` | Datum di riferimento | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `geometry.leveling_reference` | Riferimento di livellamento | TEXT | — | REQUIRED | OPERATIONAL |
| `geometry.control_surface_geometry` | Geometria superfici di controllo | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `geometry.antenna_sensor_locations` | Posizioni antenne e sensori | TABLE | — | CONDITIONAL | OPERATIONAL |

## Masse e centraggio (`mass_balance`) — 18

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `mass_balance.empty_mass` | Massa a vuoto | NUMBER | kg | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.basic_empty_mass` | Massa a vuoto operativa | NUMBER | kg | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.minimum_flight_mass` | Massa minima di volo | NUMBER | kg | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.maximum_takeoff_mass` | Massa massima al decollo | NUMBER | kg | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.maximum_landing_mass` | Massa massima all'atterraggio | NUMBER | kg | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.maximum_zero_fuel_mass` | Massa massima senza carburante | NUMBER | kg | CONDITIONAL | SAFETY_CRITICAL |
| `mass_balance.maximum_baggage_mass` | Massa massima bagaglio | NUMBER | kg | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.minimum_seat_mass` | Massa minima sedile | NUMBER | kg | CONDITIONAL | SAFETY_CRITICAL |
| `mass_balance.maximum_seat_mass` | Massa massima sedile | NUMBER | kg | CONDITIONAL | SAFETY_CRITICAL |
| `mass_balance.fuel_density_reference` | Densità carburante di riferimento | NUMBER | kg/L | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.empty_cg` | Baricentro a vuoto | NUMBER | mm | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.cg_envelope` | Inviluppo di centraggio | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.loading_stations` | Stazioni di carico e bracci | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.standard_equipment_list` | Lista equipaggiamenti di pesata | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.ballast_rules` | Regole zavorra | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `mass_balance.weighing_date` | Data ultima pesata | DATE | — | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.weighing_report_reference` | Riferimento rapporto pesata | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `mass_balance.loading_examples` | Esempi di carico approvati | TABLE | — | OPTIONAL | SAFETY_CRITICAL |

## Limiti strutturali e inviluppo (`structural_limits`) — 16

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `structural_limits.positive_load_factor_clean` | Fattore di carico positivo pulito | NUMBER | g | REQUIRED | SAFETY_CRITICAL |
| `structural_limits.negative_load_factor_clean` | Fattore di carico negativo pulito | NUMBER | g | REQUIRED | SAFETY_CRITICAL |
| `structural_limits.positive_load_factor_flaps` | Fattore positivo con flap | NUMBER | g | REQUIRED | SAFETY_CRITICAL |
| `structural_limits.negative_load_factor_flaps` | Fattore negativo con flap | NUMBER | g | CONDITIONAL | SAFETY_CRITICAL |
| `structural_limits.maximum_operating_altitude` | Quota operativa massima | NUMBER | ft | REQUIRED | SAFETY_CRITICAL |
| `structural_limits.temperature_envelope` | Inviluppo temperatura esterna | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `structural_limits.maximum_demonstrated_crosswind` | Vento traverso massimo dimostrato | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `structural_limits.maximum_tailwind` | Componente in coda massima | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `structural_limits.maximum_bank_normal` | Bank massimo normale | NUMBER | deg | CONDITIONAL | SAFETY_CRITICAL |
| `structural_limits.maximum_pitch_normal` | Pitch massimo normale | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `structural_limits.prohibited_manoeuvres` | Manovre vietate | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `structural_limits.approved_manoeuvres` | Manovre ammesse | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `structural_limits.icing_limitations` | Limitazioni ghiaccio | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `structural_limits.precipitation_limitations` | Limitazioni precipitazioni | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `structural_limits.turbulence_limitations` | Limitazioni turbolenza | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `structural_limits.negative_g_limitations` | Limitazioni accelerazioni negative | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |

## Velocità operative (`speeds`) — 25

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `speeds.vso` | VSO | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.vs1` | VS1 | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.stall_speed_table` | Velocità di stallo per massa/configurazione | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `speeds.vfe_by_flap` | VFE per posizione flap | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `speeds.vx` | VX | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.vy` | VY | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.va_by_mass` | VA per massa | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `speeds.vno` | VNO | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.vne` | VNE | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.vra` | VRA turbolenza | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `speeds.vle` | VLE | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `speeds.vlo_extend` | VLO estensione | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `speeds.vlo_retract` | VLO retrazione | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `speeds.best_glide` | Velocità miglior planata | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.best_glide_by_mass` | Best glide per massa | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `speeds.minimum_sink` | Velocità minima caduta | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `speeds.vref` | VREF | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.approach_speed_table` | Velocità avvicinamento per massa/flap/vento | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `speeds.rotation_speed` | Velocità rotazione | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `speeds.climb_speed` | Velocità salita normale | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.cruise_speed` | Velocità crociera di riferimento | NUMBER | kt | REQUIRED | SAFETY_CRITICAL |
| `speeds.maximum_tow_speed` | Velocità massima traino | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `speeds.maximum_door_window_open_speed` | Velocità massima aperture | NUMBER | kt | CONDITIONAL | SAFETY_CRITICAL |
| `speeds.airspeed_indicator_arcs` | Archi anemometrici | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `speeds.speed_corrections` | Correzioni IAS/CAS/TAS | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |

## Prestazioni (`performance`) — 20

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `performance.takeoff_ground_roll` | Corsa decollo | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `performance.takeoff_over_obstacle` | Decollo su ostacolo | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `performance.takeoff_corrections` | Correzioni decollo | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `performance.landing_ground_roll` | Corsa atterraggio | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `performance.landing_over_obstacle` | Atterraggio su ostacolo | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `performance.landing_corrections` | Correzioni atterraggio | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `performance.climb_rate_table` | Rateo salita | TABLE | — | REQUIRED | OPERATIONAL |
| `performance.time_fuel_distance_to_climb` | Tempo carburante distanza salita | TABLE | — | REQUIRED | OPERATIONAL |
| `performance.cruise_table` | Prestazioni crociera | TABLE | — | REQUIRED | OPERATIONAL |
| `performance.range_table` | Autonomia chilometrica | TABLE | — | REQUIRED | OPERATIONAL |
| `performance.endurance_table` | Autonomia oraria | TABLE | — | REQUIRED | OPERATIONAL |
| `performance.service_ceiling` | Tangenza pratica | NUMBER | ft | REQUIRED | OPERATIONAL |
| `performance.absolute_ceiling` | Tangenza assoluta | NUMBER | ft | OPTIONAL | OPERATIONAL |
| `performance.glide_ratio` | Efficienza massima | NUMBER | 1 | REQUIRED | SAFETY_CRITICAL |
| `performance.glide_polar` | Polare di planata | CURVE | — | REQUIRED | SAFETY_CRITICAL |
| `performance.turn_sink_penalty` | Perdita in virata | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `performance.turnback_envelope` | Inviluppo rientro dopo decollo | TABLE | — | OPTIONAL | SAFETY_CRITICAL |
| `performance.density_altitude_corrections` | Correzioni density altitude | TABLE | — | REQUIRED | OPERATIONAL |
| `performance.wind_corrections` | Correzioni vento | TABLE | — | REQUIRED | OPERATIONAL |
| `performance.performance_assumptions` | Assunzioni tabelle prestazioni | TEXT | — | REQUIRED | OPERATIONAL |

## Motore (`engine`) — 31

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `engine.manufacturer` | Costruttore motore | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `engine.model` | Modello motore | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `engine.variant` | Variante motore | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `engine.serial_number` | Numero di serie motore | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `engine.cylinders` | Numero cilindri | INTEGER | — | REQUIRED | SAFETY_CRITICAL |
| `engine.displacement` | Cilindrata | NUMBER | cm3 | REQUIRED | SAFETY_CRITICAL |
| `engine.compression_ratio` | Rapporto compressione | NUMBER | 1 | REQUIRED | SAFETY_CRITICAL |
| `engine.rated_power` | Potenza nominale | NUMBER | hp | REQUIRED | SAFETY_CRITICAL |
| `engine.takeoff_power` | Potenza decollo | NUMBER | hp | REQUIRED | SAFETY_CRITICAL |
| `engine.takeoff_rpm` | Regime decollo | NUMBER | rpm | REQUIRED | SAFETY_CRITICAL |
| `engine.takeoff_time_limit` | Limite temporale decollo | DURATION | s | REQUIRED | SAFETY_CRITICAL |
| `engine.maximum_continuous_power` | Potenza massima continua | NUMBER | hp | REQUIRED | SAFETY_CRITICAL |
| `engine.maximum_continuous_rpm` | Regime massimo continuo | NUMBER | rpm | REQUIRED | SAFETY_CRITICAL |
| `engine.idle_rpm_range` | Intervallo minimo | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `engine.warmup_requirements` | Requisiti riscaldamento | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `engine.cooldown_requirements` | Requisiti raffreddamento | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `engine.oil_pressure_limits` | Limiti pressione olio | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `engine.oil_temperature_limits` | Limiti temperatura olio | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `engine.coolant_temperature_limits` | Limiti refrigerante | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `engine.cylinder_head_temperature_limits` | Limiti temperatura teste | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `engine.egt_limits` | Limiti EGT | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `engine.fuel_pressure_limits` | Limiti pressione carburante | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `engine.manifold_pressure_limits` | Limiti MAP | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `engine.fuel_flow_limits` | Limiti fuel flow | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `engine.vibration_limits` | Limiti vibrazione | TABLE | — | OPTIONAL | SAFETY_CRITICAL |
| `engine.negative_g_operation` | Funzionamento in G negativi | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `engine.tbo` | TBO applicabile | DURATION | h | REQUIRED | MAINTENANCE |
| `engine.calendar_tbo` | TBO calendario | DURATION | month | REQUIRED | MAINTENANCE |
| `engine.operating_manual` | Manuale operatore applicabile | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `engine.installation_manual` | Manuale installazione applicabile | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `engine.service_document_applicability` | SB SI SL applicabili | TABLE | — | REQUIRED | MAINTENANCE |

## Carburanti lubrificanti e refrigeranti (`fluids`) — 10

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `fluids.approved_fuels` | Carburanti approvati | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `fluids.fuel_additive_limits` | Additivi carburante | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `fluids.approved_oils` | Oli approvati | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `fluids.oil_capacity_min` | Capacità olio minima | NUMBER | L | REQUIRED | SAFETY_CRITICAL |
| `fluids.oil_capacity_max` | Capacità olio massima | NUMBER | L | REQUIRED | SAFETY_CRITICAL |
| `fluids.maximum_oil_consumption` | Consumo olio massimo | NUMBER | L/h | REQUIRED | SAFETY_CRITICAL |
| `fluids.approved_coolants` | Refrigeranti approvati | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `fluids.coolant_capacity` | Capacità refrigerante | NUMBER | L | CONDITIONAL | SAFETY_CRITICAL |
| `fluids.brake_fluid` | Fluido freni | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `fluids.hydraulic_fluid` | Fluido idraulico | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |

## Elica e governor (`propeller`) — 14

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `propeller.manufacturer` | Costruttore elica | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `propeller.model` | Modello elica | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `propeller.serial_number` | Numero serie elica | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `propeller.blade_count` | Numero pale | INTEGER | — | REQUIRED | SAFETY_CRITICAL |
| `propeller.diameter` | Diametro | NUMBER | m | REQUIRED | SAFETY_CRITICAL |
| `propeller.pitch_control_type` | Tipo controllo passo | ENUM | — | REQUIRED | SAFETY_CRITICAL |
| `propeller.pitch_range` | Intervallo passo | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `propeller.rpm_limits` | Limiti RPM elica | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `propeller.operating_restrictions` | Restrizioni operative elica | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `propeller.governor_model` | Modello governor | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `propeller.feather_available` | Messa in bandiera | BOOLEAN | — | CONDITIONAL | SAFETY_CRITICAL |
| `propeller.reverse_available` | Reverse disponibile | BOOLEAN | — | CONDITIONAL | SAFETY_CRITICAL |
| `propeller.inertia` | Momento inerzia | NUMBER | kg*m2 | CONDITIONAL | SAFETY_CRITICAL |
| `propeller.manual_reference` | Manuale elica applicabile | TEXT | — | REQUIRED | SAFETY_CRITICAL |

## Impianto carburante (`fuel_system`) — 16

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `fuel_system.tank_inventory` | Serbatoi e posizioni | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.total_capacity` | Capacità totale | NUMBER | L | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.total_usable` | Carburante totale utilizzabile | NUMBER | L | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.total_unusable` | Carburante non utilizzabile | NUMBER | L | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.selector_positions` | Posizioni selettore | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.feed_logic` | Logica alimentazione | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.mechanical_pump` | Pompa meccanica | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.electric_pumps` | Pompe elettriche | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `fuel_system.crossfeed` | Crossfeed | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `fuel_system.maximum_imbalance` | Sbilanciamento massimo | NUMBER | L | CONDITIONAL | SAFETY_CRITICAL |
| `fuel_system.venting` | Sfiati serbatoi | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.drain_points` | Punti drenaggio | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.level_sensor_calibration` | Calibrazione sensori livello | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.low_fuel_thresholds` | Soglie basso carburante | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.refuel_procedure` | Procedura rifornimento | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `fuel_system.fuel_return_system` | Ritorno carburante | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |

## Impianto elettrico (`electrical`) — 19

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `electrical.nominal_voltage` | Tensione nominale | NUMBER | V | REQUIRED | SAFETY_CRITICAL |
| `electrical.operating_voltage_limits` | Limiti tensione operativa | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.battery_inventory` | Batterie | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.generator_alternator_inventory` | Generatori e alternatori | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.external_power` | Alimentazione esterna | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `electrical.bus_architecture` | Architettura bus | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.source_switching_logic` | Commutazione sorgenti | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.essential_loads` | Utenze essenziali | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.load_shed_sequence` | Sequenza load shedding | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.circuit_protection` | Fusibili e interruttori | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.wire_connector_schedule` | Cablaggi e connettori | TABLE | — | REQUIRED | MAINTENANCE |
| `electrical.grounding_bonding` | Masse e bonding | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.starter_current` | Corrente motorino avviamento | NUMBER | A | CONDITIONAL | SAFETY_CRITICAL |
| `electrical.alternator_current_limit` | Corrente alternatore massima | NUMBER | A | REQUIRED | SAFETY_CRITICAL |
| `electrical.low_voltage_thresholds` | Soglie bassa tensione | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.high_voltage_thresholds` | Soglie sovratensione | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.essential_endurance` | Autonomia bus essenziale | DURATION | min | CONDITIONAL | SAFETY_CRITICAL |
| `electrical.brownout_behavior` | Comportamento brownout | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `electrical.schematic_reference` | Riferimento schema elettrico | TEXT | — | REQUIRED | SAFETY_CRITICAL |

## Comandi di volo e trim (`flight_controls`) — 15

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `flight_controls.architecture` | Architettura comandi | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `flight_controls.aileron_travel` | Corsa alettoni | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `flight_controls.elevator_travel` | Corsa elevatore | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `flight_controls.rudder_travel` | Corsa direzionale | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `flight_controls.control_direction_check` | Verifica direzione comandi | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `flight_controls.control_stop_settings` | Regolazione fine corsa | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `flight_controls.cable_tensions` | Tensioni cavi | TABLE | — | CONDITIONAL | MAINTENANCE |
| `flight_controls.control_free_play_limits` | Limiti giochi comandi | TABLE | — | REQUIRED | MAINTENANCE |
| `flight_controls.roll_trim` | Trim rollio | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `flight_controls.pitch_trim` | Trim beccheggio | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `flight_controls.yaw_trim` | Trim imbardata | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `flight_controls.trim_ranges` | Campi trim | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `flight_controls.trim_runaway_procedure_ref` | Procedura trim runaway | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `flight_controls.control_lock` | Blocco comandi | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `flight_controls.manual_override_forces` | Forze override manuale | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |

## Flap e ipersostentatori (`flaps`) — 10

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `flaps.installed` | Flap installati | BOOLEAN | — | CONDITIONAL | SAFETY_CRITICAL |
| `flaps.actuation_type` | Tipo azionamento flap | ENUM | — | CONDITIONAL | SAFETY_CRITICAL |
| `flaps.detents` | Posizioni e angoli | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `flaps.travel_limits` | Limiti corsa | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `flaps.extension_retraction_times` | Tempi movimento | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `flaps.position_sensor` | Sensore posizione | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `flaps.asymmetry_detection` | Rilevamento asimmetria | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `flaps.failure_procedure_ref` | Procedura avaria flap | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `flaps.takeoff_settings` | Configurazioni decollo | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `flaps.landing_settings` | Configurazioni atterraggio | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |

## Carrello sterzo e freni (`landing_gear`) — 15

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `landing_gear.type` | Tipo carrello | ENUM | — | REQUIRED | SAFETY_CRITICAL |
| `landing_gear.actuation_type` | Tipo azionamento | ENUM | — | REQUIRED | SAFETY_CRITICAL |
| `landing_gear.extension_time` | Tempo estensione | DURATION | s | CONDITIONAL | SAFETY_CRITICAL |
| `landing_gear.retraction_time` | Tempo retrazione | DURATION | s | CONDITIONAL | SAFETY_CRITICAL |
| `landing_gear.position_indication` | Indicazione posizione | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `landing_gear.downlock_logic` | Logica downlock | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `landing_gear.uplock_logic` | Logica uplock | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `landing_gear.warning_logic` | Logica avviso carrello | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `landing_gear.emergency_extension` | Estensione emergenza | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `landing_gear.tire_specifications` | Pneumatici | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `landing_gear.tire_pressures` | Pressioni pneumatici | TABLE | — | REQUIRED | MAINTENANCE |
| `landing_gear.brake_type` | Tipo freni | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `landing_gear.brake_limits` | Limiti freni | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `landing_gear.parking_brake` | Freno parcheggio | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `landing_gear.nose_tail_steering` | Sterzo a terra | TEXT | — | REQUIRED | SAFETY_CRITICAL |

## Pitot statica e air-data (`pitot_static_airdata`) — 12

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `pitot_static_airdata.pitot_type` | Tipo pitot | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `pitot_static_airdata.pitot_location` | Posizione pitot | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `pitot_static_airdata.static_port_locations` | Prese statiche | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `pitot_static_airdata.pitot_heat` | Riscaldamento pitot | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `pitot_static_airdata.alternate_static_source` | Statica alternativa | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `pitot_static_airdata.airspeed_calibration` | Calibrazione anemometrica | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `pitot_static_airdata.altimeter_calibration` | Calibrazione altimetro | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `pitot_static_airdata.vertical_speed_calibration` | Calibrazione variometro | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `pitot_static_airdata.aoa_system` | Sistema AoA | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `pitot_static_airdata.stall_warning` | Avvisatore stallo | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `pitot_static_airdata.airspeed_source_priority` | Priorità sorgenti airspeed | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `pitot_static_airdata.airdata_failure_rules` | Regole guasto air-data | TABLE | — | REQUIRED | SAFETY_CRITICAL |

## Sensori e calibrazioni (`sensors`) — 12

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `sensors.inventory` | Inventario sensori | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `sensors.channel_mapping` | Mappa canali | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `sensors.calibration_records` | Record calibrazione | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `sensors.calibration_intervals` | Intervalli calibrazione | TABLE | — | REQUIRED | MAINTENANCE |
| `sensors.validity_thresholds` | Soglie validità | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `sensors.stale_lost_thresholds` | Timeout stale/lost | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `sensors.stasis_thresholds` | Soglie stasi | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `sensors.cross_check_thresholds` | Soglie confronto incrociato | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `sensors.source_priority` | Priorità sorgenti | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `sensors.failure_annunciation` | Annunci guasto sensore | TABLE | — | REQUIRED | OPERATIONAL |
| `sensors.installation_orientation` | Orientamenti installazione | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `sensors.environmental_limits` | Limiti ambientali sensori | TABLE | — | CONDITIONAL | OPERATIONAL |

## Avionica comunicazione e navigazione (`avionics`) — 16

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `avionics.equipment_inventory` | Inventario avionica | TABLE | — | REQUIRED | OPERATIONAL |
| `avionics.com_radios` | Radio COM | TABLE | — | CONDITIONAL | OPERATIONAL |
| `avionics.nav_radios` | Radio NAV | TABLE | — | CONDITIONAL | OPERATIONAL |
| `avionics.transponder` | Transponder | JSON | — | CONDITIONAL | OPERATIONAL |
| `avionics.adsb` | ADS-B | JSON | — | CONDITIONAL | OPERATIONAL |
| `avionics.elt_plb` | ELT o PLB | TABLE | — | CONDITIONAL | OPERATIONAL |
| `avionics.gnss_receivers` | Ricevitori GNSS | TABLE | — | CONDITIONAL | OPERATIONAL |
| `avionics.magnetometers` | Magnetometri | TABLE | — | CONDITIONAL | OPERATIONAL |
| `avionics.compass_deviation` | Tabella deviazione bussola | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `avionics.audio_panel` | Pannello audio | JSON | — | CONDITIONAL | OPERATIONAL |
| `avionics.intercom` | Interfono | JSON | — | CONDITIONAL | OPERATIONAL |
| `avionics.navigation_database` | Database navigazione | JSON | — | CONDITIONAL | OPERATIONAL |
| `avionics.terrain_database` | Database terreno | JSON | — | CONDITIONAL | OPERATIONAL |
| `avionics.obstacle_database` | Database ostacoli | JSON | — | CONDITIONAL | OPERATIONAL |
| `avionics.weather_sources` | Sorgenti meteo | TABLE | — | CONDITIONAL | OPERATIONAL |
| `avionics.software_firmware_baseline` | Baseline software e firmware | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |

## Autopilota e attuatori (`autopilot`) — 19

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `autopilot.installed` | Autopilota installato | BOOLEAN | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.controller_identity` | Controllore autopilota | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.approved_modes` | Modalità approvate | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.mode_limitations` | Limitazioni modalità | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.engagement_envelope` | Inviluppo inserimento | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.disconnect_controls` | Comandi disconnessione | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.master_controls` | Comandi AP master | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.servo_inventory` | Inventario servi | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.servo_power_branches` | Rami alimentazione servi | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.servo_torque_limits` | Limiti coppia servi | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.servo_clutch_override` | Frizione e override servi | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.surface_mapping` | Mappa servo-superfici | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.control_gains` | Guadagni controllo approvati | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.tecs_parameters` | Parametri TECS approvati | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.airspeed_parameters` | Parametri airspeed controllo | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.failsafe_configuration` | Configurazione failsafe | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.parameter_baseline_hash` | Hash parametri autopilota | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.hil_acceptance_reference` | Riferimento accettazione HIL | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |
| `autopilot.flight_test_acceptance_reference` | Riferimento prove volo | TEXT | — | CONDITIONAL | SAFETY_CRITICAL |

## Sistemi opzionali e cabina (`optional_systems`) — 12

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `optional_systems.ballistic_parachute` | Paracadute balistico | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `optional_systems.oxygen_system` | Impianto ossigeno | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `optional_systems.anti_ice_deice` | Anti-ice e de-ice | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `optional_systems.carburetor_heat` | Riscaldamento carburatori | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `optional_systems.cabin_heat` | Riscaldamento cabina | JSON | — | CONDITIONAL | OPERATIONAL |
| `optional_systems.ventilation` | Ventilazione cabina | JSON | — | CONDITIONAL | OPERATIONAL |
| `optional_systems.doors_canopy` | Porte e tettuccio | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `optional_systems.exterior_lighting` | Luci esterne | TABLE | — | CONDITIONAL | OPERATIONAL |
| `optional_systems.interior_lighting` | Luci interne | TABLE | — | CONDITIONAL | OPERATIONAL |
| `optional_systems.fire_detection_extinguishing` | Rilevamento o estinzione incendio | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `optional_systems.camera_system` | Sistema telecamere | TABLE | — | CONDITIONAL | OPERATIONAL |
| `optional_systems.icing_detection` | Rilevamento ghiaccio | JSON | — | CONDITIONAL | SAFETY_CRITICAL |

## Procedure normali (`normal_procedures`) — 16

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `normal_procedures.preflight` | Ispezione prevolo | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.before_start` | Prima avviamento | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.engine_start` | Avviamento motore | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.after_start` | Dopo avviamento | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.taxi` | Rullaggio | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.runup` | Prova motore | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.before_takeoff` | Prima del decollo | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.takeoff` | Decollo | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.climb` | Salita | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.cruise` | Crociera | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.descent` | Discesa | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.before_landing` | Prima atterraggio | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.landing` | Atterraggio | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.after_landing` | Dopo atterraggio | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.shutdown` | Spegnimento | JSON | — | REQUIRED | OPERATIONAL |
| `normal_procedures.securing` | Messa in sicurezza | JSON | — | REQUIRED | OPERATIONAL |

## Procedure anormali e di emergenza (`emergency_procedures`) — 34

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `emergency_procedures.engine_failure_takeoff_run` | Avaria motore corsa decollo | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.engine_failure_after_takeoff` | Avaria motore dopo decollo | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.engine_failure_in_flight` | Avaria motore in volo | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.engine_restart` | Riavviamento in volo | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.engine_fire_start_ground` | Incendio motore avviamento o terra | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.engine_fire_flight` | Incendio motore in volo | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.cabin_electrical_fire` | Fumo o incendio cabina/elettrico | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.rough_running_power_loss` | Funzionamento irregolare o potenza parziale | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.low_oil_pressure` | Bassa pressione olio | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.high_temperature` | Sovratemperatura | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.fuel_pressure_failure` | Avaria pressione carburante | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.alternator_failure` | Avaria alternatore | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.battery_or_bus_failure` | Avaria batteria o bus | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.forced_landing` | Atterraggio forzato | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.precautionary_landing` | Atterraggio precauzionale | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.ditching` | Ammaraggio | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.stall_recovery` | Rimessa dallo stallo | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.spin_recovery` | Rimessa dalla vite | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.unusual_attitude` | Assetto inusuale | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.control_jam` | Blocco comandi | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.trim_runaway` | Trim runaway | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.flap_failure` | Avaria flap | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.gear_failure` | Avaria carrello | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.brake_failure` | Avaria freni | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.canopy_door_open` | Tettuccio o porta aperta | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.inadvertent_imc` | Ingresso involontario IMC | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.pitot_static_failure` | Avaria pitot/statica | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.primary_display_failure` | Avaria display primario | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.navigation_failure` | Avaria navigazione | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.radio_failure` | Avaria radio | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_procedures.autopilot_failure` | Avaria autopilota | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.ballistic_parachute_use` | Uso paracadute balistico | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.pilot_incapacitation` | Incapacità pilota | JSON | — | CONDITIONAL | SAFETY_CRITICAL |
| `emergency_procedures.emergency_evacuation` | Evacuazione emergenza | JSON | — | REQUIRED | SAFETY_CRITICAL |

## EMS CAS e soglie (`ems_alerts`) — 10

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `ems_alerts.engine_sensor_channels` | Canali sensori motore | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `ems_alerts.normal_ranges` | Intervalli normali | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `ems_alerts.caution_ranges` | Intervalli cautela | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `ems_alerts.warning_ranges` | Intervalli warning | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `ems_alerts.time_delays_hysteresis` | Ritardi e isteresi | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `ems_alerts.rate_of_change_alerts` | Allarmi variazione rapida | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `ems_alerts.cylinder_spread_limits` | Limiti differenza cilindri | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `ems_alerts.leaning_rules` | Regole leaning | TABLE | — | CONDITIONAL | SAFETY_CRITICAL |
| `ems_alerts.cas_messages` | Messaggi CAS | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `ems_alerts.aural_messages` | Messaggi vocali EMS | TABLE | — | REQUIRED | SAFETY_CRITICAL |

## Configurazione Emergency Assist (`emergency_assist`) — 18

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `emergency_assist.enabled_scenarios` | Scenari abilitati | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.procedure_package` | Pacchetto procedure approvato | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.best_glide_source` | Fonte best glide | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.conservative_glide_model` | Modello planata conservativo | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.terrain_clearance_reserve` | Riserva separazione terreno | NUMBER | ft | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.arrival_altitude_reserve` | Riserva quota arrivo | NUMBER | ft | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.model_uncertainty` | Margine incertezza modello | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.airport_hard_gates` | Gate obbligatori aeroporto | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.airport_ranking_weights` | Pesi ranking aeroporti | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.off_airport_policy` | Politica fuori campo | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.takeoff_turnback_policy` | Politica inversione dopo decollo | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.automatic_detection_rules` | Regole rilevamento automatico | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.data_validity_gates` | Gate validità dati | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.voice_annunciations` | Annunci vocali | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.physical_controls` | Comandi fisici Emergency | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.blackbox_event_contract` | Contratto eventi Blackbox | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.human_factors_acceptance` | Accettazione human factors | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `emergency_assist.hil_acceptance` | Accettazione HIL | TEXT | — | REQUIRED | SAFETY_CRITICAL |

## Manutenzione e vita limite (`maintenance`) — 15

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `maintenance.maintenance_program` | Programma manutenzione | TEXT | — | REQUIRED | MAINTENANCE |
| `maintenance.inspection_intervals` | Intervalli ispezione | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.component_life_limits` | Vite limite componenti | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.engine_maintenance_intervals` | Intervalli motore | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.propeller_maintenance_intervals` | Intervalli elica | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.landing_gear_maintenance` | Manutenzione carrello | TABLE | — | CONDITIONAL | MAINTENANCE |
| `maintenance.control_system_maintenance` | Manutenzione comandi | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.lubrication_schedule` | Piano lubrificazione | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.torque_schedule` | Coppie di serraggio | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.ad_sb_si_status` | Stato AD SB SI | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.recurring_calibrations` | Calibrazioni ricorrenti | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.software_update_policy` | Politica aggiornamento software | TEXT | — | REQUIRED | MAINTENANCE |
| `maintenance.configuration_audit_interval` | Intervallo audit configurazione | DURATION | month | REQUIRED | MAINTENANCE |
| `maintenance.logbook_references` | Riferimenti libretti | TABLE | — | REQUIRED | MAINTENANCE |
| `maintenance.storage_preservation` | Preservazione e rimessaggio | TEXT | — | REQUIRED | MAINTENANCE |

## Unità e presentazione (`operating_units`) — 12

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `operating_units.airspeed` | Unità velocità aria | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.altitude` | Unità quota | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.vertical_speed` | Unità velocità verticale | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.pressure` | Unità pressione barometrica | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.temperature` | Unità temperatura | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.fuel_volume` | Unità volume carburante | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.fuel_flow` | Unità flusso carburante | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.mass` | Unità massa | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.distance` | Unità distanza | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.engine_pressure` | Unità pressioni motore | ENUM | — | REQUIRED | OPERATIONAL |
| `operating_units.language` | Lingua operativa | TEXT | — | REQUIRED | OPERATIONAL |
| `operating_units.time_format` | Formato orario | ENUM | — | REQUIRED | OPERATIONAL |

## Provenienza validazione e rilascio (`data_governance`) — 12

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `data_governance.aircraft_poh` | POH aeromobile | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.engine_manual` | Manuale motore | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.propeller_manual` | Manuale elica | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.supplements` | Supplementi applicabili | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.effective_pages` | Pagine effettive | TABLE | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.source_manifest_hash` | Hash manifesto fonti | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.field_validation_summary` | Riepilogo validazione campi | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.owner_approval` | Approvazione proprietario | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.technical_review` | Revisione tecnica | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.release_record` | Record rilascio | JSON | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.rollback_reference` | Riferimento rollback | TEXT | — | REQUIRED | SAFETY_CRITICAL |
| `data_governance.unmapped_source_fields` | Campi fonte non mappati | TABLE | — | REQUIRED | SAFETY_CRITICAL |

## Estensioni specifiche (`custom`) — 4

| Parameter | Label | Type | Unit | Requirement | Criticality |
|---|---|---|---|---|---|
| `custom.aircraft_specific_parameters` | Parametri specifici aeromobile | JSON | — | REQUIRED | OPERATIONAL |
| `custom.engine_specific_parameters` | Parametri specifici motore | JSON | — | REQUIRED | OPERATIONAL |
| `custom.equipment_specific_parameters` | Parametri specifici equipaggiamenti | JSON | — | REQUIRED | OPERATIONAL |
| `custom.open_configuration_items` | Elementi configurazione aperti | TABLE | — | REQUIRED | SAFETY_CRITICAL |
