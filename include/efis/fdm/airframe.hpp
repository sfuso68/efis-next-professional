#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace efis::fdm {

// Cellula e aerodinamica.
//
// Il modello e' a coefficienti adimensionali con derivate di stabilita': la
// forma classica della meccanica del volo, quella dei testi e dei rapporti di
// galleria. Non e' la sola possibile -- un simulatore moderno integra le forze
// su elementi di superficie -- ma e' quella che si puo' tarare con i numeri che
// un costruttore LSA possiede davvero (superficie alare, apertura, pesi,
// velocita' di stallo misurate) e che si puo' leggere e discutere riga per
// riga. Un modello a elementi di superficie richiede una geometria che qui non
// esiste e produrrebbe numeri non verificabili.
//
// Stato: DEVELOPMENT. I coefficienti sono scelti con criterio fisico e scalati
// su macchine di classe nota, non misurati sulle macchine vere. Servono a far
// volare un aeroplano plausibile per provare un EFIS, non a stimare prestazioni.

enum class Category : std::uint8_t {
    // LSA e ultraleggeri: velocita' di stallo bassa per definizione, nessuna
    // pressurizzazione, comandi semplici.
    light_sport,
    // Classe professionale: turboelica pressurizzata, carrello retrattile,
    // quote dove la differenza fra velocita' calibrata e vera e' grande.
    professional
};

// Geometria che entra nelle forze. Nient'altro: tutto il resto della forma
// dell'aeroplano e' gia' dentro i coefficienti.
struct Geometry final {
    double wing_area_m2{9.9};
    double wing_span_m{8.4};
    double mean_chord_m{1.2};

    [[nodiscard]] double aspect_ratio() const noexcept {
        return wing_span_m * wing_span_m / wing_area_m2;
    }
};

// Masse e inerzie.
//
// Le inerzie sono stime, marcate DEVELOPMENT: discendono da raggi di girazione
// tipici per la classe, ancorati ai valori pubblicati del Cessna 172 scalati
// sulla massa. Decidono la rapidita' della risposta ai comandi, non l'assetto
// di equilibrio: un errore del venti per cento rende la macchina un po' piu' o
// un po' meno pronta, e non sposta ne' la velocita' di stallo ne' la quota di
// crociera.
struct MassProperties final {
    double empty_mass_kg{330.0};
    double max_mass_kg{600.0};
    double fuel_capacity_kg{70.0};
    double inertia_xx_kg_m2{950.0};
    double inertia_yy_kg_m2{760.0};
    double inertia_zz_kg_m2{1320.0};
};

// Derivate aerodinamiche, per radiante e adimensionalizzate secondo la
// convenzione usuale (semiapertura per i momenti di rollio e imbardata, corda
// media per quello di beccheggio).
struct AeroCoefficients final {
    // Portanza. La pendenza e' quella del velivolo completo, non del profilo:
    // l'allungamento finito la riduce, e il valore qui la contiene gia'.
    double lift_zero{0.25};
    double lift_alpha{5.2};

    // Portanza dovuta all'equilibratore, negativa perche' il comando e'
    // positivo a cabrare: un equilibratore a cabrare carica la coda verso il
    // basso e toglie portanza. Il guadagno di portanza che il pilota ottiene
    // cabrando arriva dall'incidenza, non dalla superficie, e passa quindi per
    // lift_alpha un momento dopo.
    double lift_elevator{-0.14};
    double lift_pitch_rate{3.9};

    // Incidenza a cui la portanza smette di crescere.
    //
    // Il coefficiente di portanza massimo non e' dichiarato: discende da
    // questa incidenza e dalla pendenza, e dichiararlo a parte darebbe due
    // numeri capaci di contraddirsi, con il risultato che la velocita' di
    // stallo scritta sul nastro non sarebbe quella a cui l'aeroplano stalla.
    // Si ottiene con max_lift_coefficient().
    double stall_alpha_deg{13.5};

    // Resistenza: parassita piu' indotta, con il fattore di Oswald a tenere
    // conto della distribuzione di portanza non ellittica.
    double drag_zero{0.030};
    double oswald_efficiency{0.78};

    // Momento di beccheggio. Il segno negativo della derivata rispetto
    // all'incidenza e' la stabilita' statica longitudinale: senza di essa
    // l'aeroplano non tiene un assetto e nessun pilota automatico lo recupera.
    double pitch_zero{0.04};
    double pitch_alpha{-0.75};
    double pitch_rate{-9.0};
    // Positivo perche' il comando e' positivo a cabrare. La convenzione
    // aerodinamica dei testi e' l'opposta -- deflessione positiva verso il
    // basso, momento negativo -- e qui si sceglie quella del pilota: i comandi
    // di Controls hanno il segno del gesto, cosi' chi scrive uno scenario non
    // deve ricordare una doppia inversione.
    // Il valore e' per escursione piena del comando, non per radiante di
    // deflessione: il comando di Controls vale da meno uno a piu' uno, e
    // un'escursione piena sono circa venticinque gradi, cioe' 0,44 radianti.
    // Una derivata di 1,2 per radiante diventa quindi 0,53 per escursione.
    // Scriverla per radiante, come fanno i testi, dava un equilibratore due
    // volte e mezzo troppo potente: nello scenario di stallo l'aeroplano
    // cabrava a quaranta gradi in cinque secondi.
    double pitch_elevator{0.53};

    // Forza laterale e momenti di rollio e imbardata. Il diedro effettivo
    // (roll_sideslip negativo) e la stabilita' direzionale (yaw_sideslip
    // positivo) sono cio' che rende l'aeroplano stabile in spirale e cio' che
    // fa funzionare la pallina.
    //
    // Le derivate rispetto alla derapata e ai ratei sono per radiante, come nei
    // testi. Quelle rispetto ai comandi no: sono per escursione piena, come
    // pitch_elevator, perche' il comando vale da meno uno a piu' uno. Il
    // fattore e' l'escursione angolare della superficie, circa 0,44 radianti
    // per alettoni ed equilibratore e 0,52 per il timone. Trascurarlo era un
    // errore che si vedeva soltanto in virata: l'imbardata inversa
    // dell'alettone, quattro volte troppo grande, dava alla rollata un rateo
    // di prua di quattordici gradi al secondo, dove la relazione della virata
    // corretta ne prevede tre e mezzo.
    double side_sideslip{-0.31};

    // Timone positivo a destra. La forza laterale e' negativa perche' per
    // imbardare a destra la deriva spinge la coda a sinistra.
    double side_rudder{-0.097};
    double roll_sideslip{-0.089};
    double roll_rate{-0.47};
    double roll_yaw_rate{0.096};
    double roll_aileron{0.100};
    double roll_rudder{0.0076};
    double yaw_sideslip{0.065};
    double yaw_roll_rate{-0.03};
    double yaw_rate{-0.099};
    double yaw_aileron{-0.0095};
    double yaw_rudder{0.0335};
};

// Posizione del comando flap. Ogni tacca porta con se' il proprio limite di
// velocita', perche' e' quello che l'EFIS deve mostrare sul nastro e che il
// pilota supera per errore.
struct FlapDetent final {
    double angle_deg{};
    double delta_lift{};
    double delta_drag{};
    double delta_pitch{};
    double limit_kcas{};
};

// Velocita' caratteristiche, in velocita' calibrata.
//
// Non sono decorazione del nastro: sono il contratto fra la cellula e il
// display. Le velocita' di stallo dichiarate qui devono coincidere con quelle
// che il modello produce davvero, e un test lo verifica. Una cellula le cui
// velocita' dichiarate non corrispondono al proprio comportamento e' peggio di
// una cellula senza nastro, perche' l'errore non si vede.
struct SpeedLimits final {
    double stall_clean_kcas{50.0};
    double stall_landing_kcas{44.0};
    double manoeuvring_kcas{95.0};
    double normal_operating_kcas{120.0};
    double never_exceed_kcas{150.0};
    double best_climb_kcas{65.0};
    double best_glide_kcas{70.0};

    // Zero significa che il limite di Mach non si applica: un LSA non lo
    // raggiunge, e mostrarne uno sul nastro sarebbe informazione inventata.
    double max_mach{0.0};
};

enum class EngineKind : std::uint8_t { piston_prop, turboprop };

// Propulsore. Il modello sta in propulsion.hpp; qui c'e' cio' che serve a
// dimensionarlo.
struct PropulsionConfig final {
    EngineKind kind{EngineKind::piston_prop};

    // Potenza all'albero al livello del mare, a pieno regime.
    double rated_power_kw{73.5};  // 100 hp, classe Rotax 912 ULS

    // Regime nominale del motore e rapporto del riduttore. La distinzione non
    // e' pedanteria: il contagiri dell'EFIS mostra il regime motore, mentre la
    // spinta dipende da quello dell'elica, e su un Rotax i due differiscono di
    // un fattore due e mezzo.
    double rated_engine_rpm{5500.0};
    double gear_ratio{2.43};
    double propeller_diameter_m{1.72};

    // Merito dell'elica: la frazione della potenza all'albero che il disco
    // riesce a trasformare in lavoro sull'aria. Non e' il rendimento
    // propulsivo, che dipende dalla velocita' e che il modello calcola: e' la
    // perdita di profilo delle pale, che non dipende da essa. Una tabella di
    // rendimento in funzione del rapporto di funzionamento non serve, perche'
    // la teoria del disco attuatore in propulsion.cpp produce quella curva da
    // sola, spinta statica compresa.
    double propeller_peak_efficiency{0.82};

    // Rapporto di funzionamento del punto di progetto: quello a cui l'elica
    // assorbe esattamente la potenza di targa girando al regime nominale.
    //
    // Serve perche' il punto di progetto di un'elica non e' la condizione
    // statica. Tararla da ferma significa darle un passo troppo piccolo, e in
    // crociera il motore supera il regime massimo: e' l'errore che si vede
    // subito, perche' il contagiri lo mostra. Per un'elica da crociera di un
    // LSA il valore sta attorno a 0,8.
    double propeller_design_advance_ratio{0.80};

    // Consumo specifico al freno. Decide il consumo orario, che e' il numero
    // che l'EMS mostra e su cui il pilota decide quando atterrare.
    double bsfc_kg_kwh{0.28};

    // Inerzia rotante vista dall'albero dell'elica.
    //
    // Decide quanto il regime tarda a seguire la manetta, e quel ritardo non e'
    // un dettaglio estetico: e' il transitorio che l'EMS mostra e che un
    // allarme di sovragiri deve saper distinguere da un disturbo. Senza
    // inerzia il regime salterebbe al valore di equilibrio nello stesso passo
    // di integrazione, cioe' in modo che nessun motore vero puo' fare.
    double rotating_inertia_kg_m2{0.6};

    double idle_rpm{1400.0};
    double starter_rpm{300.0};
};

// Carrello: tre punti di contatto con molla e smorzatore.
//
// Esiste perche' senza di esso non si puo' provare la corsa di decollo, ed e'
// durante la corsa di decollo che la sorveglianza di ADR-004 si apre. Un banco
// che partisse in volo non eserciterebbe mai quel passaggio.
struct LandingGearConfig final {
    bool retractable{false};
    double delta_drag_extended{0.022};

    // Posizione dei contatti rispetto al baricentro, in assi corpo: x avanti,
    // y a destra, z verso il basso.
    double nose_x_m{1.30};
    double main_x_m{-0.35};
    double main_y_m{0.90};
    double contact_z_m{0.95};

    double spring_n_m{30000.0};
    double damper_n_s_m{3500.0};
    double rolling_friction{0.025};
    double brake_friction{0.55};
};

struct Airframe final {
    std::string name{"LSA generico"};
    Category category{Category::light_sport};
    Geometry geometry{};
    MassProperties mass{};
    AeroCoefficients aero{};
    std::vector<FlapDetent> flaps{};
    SpeedLimits limits{};
    PropulsionConfig propulsion{};
    LandingGearConfig gear{};

    // Calettamento dell'asse motore rispetto all'asse corpo x. Non e' un
    // dettaglio: decide quanta della spinta diventa portanza e quanto
    // l'aeroplano cabra quando si da' motore.
    double thrust_incidence_deg{2.0};
};

// Le due cellule di riferimento del banco.
//
// Due e non una perche' i due mondi a cui l'EFIS deve servire pongono problemi
// diversi, e un motore provato su una sola classe nasconde proprio quelli
// dell'altra. L'LSA vola lento, vicino allo stallo, senza pressurizzazione:
// mette alla prova il nastro delle velocita' basse, l'allarme di stallo e
// l'incidenza. La turboelica vola alta e veloce: mette alla prova la differenza
// fra velocita' calibrata e vera, il numero di Mach, la quota di densita' e la
// catena motore a turbina.
[[nodiscard]] Airframe light_sport_airframe();
[[nodiscard]] Airframe professional_airframe();

[[nodiscard]] const char* category_name(Category category) noexcept;

// --- Aerodinamica ------------------------------------------------------------

// Stato aerodinamico istantaneo. Tutto in radianti e in unita' SI: la
// conversione verso nodi e gradi appartiene allo strato che pubblica, non a
// quello che calcola.
struct AeroState final {
    double alpha_rad{};
    double sideslip_rad{};
    double roll_rate_rad_s{};
    double pitch_rate_rad_s{};
    double yaw_rate_rad_s{};
    double true_airspeed_m_s{};
    double dynamic_pressure_pa{};
};

// Posizione dei comandi. Le superfici in frazione di escursione, da -1 a +1; i
// comandi motore da 0 a 1.
//
// Il segno e' quello del gesto del pilota: equilibratore positivo a cabrare,
// alettone positivo a destra, timone positivo a destra. Le derivate in
// AeroCoefficients portano il segno conseguente, che per l'equilibratore e'
// l'opposto della convenzione dei testi. La scelta e' deliberata: un comando
// che ha il segno del gesto si legge negli scenari senza dover ricordare
// un'inversione, e l'inversione resta scritta una volta sola, dove nasce.
struct Controls final {
    double elevator{};
    double aileron{};
    double rudder{};
    double elevator_trim{};
    double throttle{};
    double propeller{1.0};
    double mixture{1.0};
    double flap_deg{};
    bool gear_down{true};
    double brake{};
    bool magnetos_on{true};
    bool starter{false};
    bool fuel_pump_on{true};
};

// Forze e momenti aerodinamici in assi corpo, piu' i due coefficienti che
// servono a chi guarda.
struct AeroLoads final {
    double force_x_n{};
    double force_y_n{};
    double force_z_n{};
    double moment_roll_nm{};
    double moment_pitch_nm{};
    double moment_yaw_nm{};
    double lift_coefficient{};
    double drag_coefficient{};

    // Vero quando l'incidenza ha superato quella di stallo. Non e' una
    // diagnosi del modello: e' la condizione che l'allarme di stallo dell'EFIS
    // deve riconoscere, e il banco esiste per produrla a comando.
    bool stalled{};
};

// Coefficiente di portanza all'incidenza data, con lo stallo.
//
// Oltre l'incidenza di stallo la relazione lineare non vale piu', e troncarla
// al massimo darebbe un aeroplano che non stalla: resta appeso al valore
// massimo e continua a volare. Il raccordo e' una miscela fra la retta e il
// comportamento a lastra piana, che oltre lo stallo e' cio' che l'ala diventa.
// Serve perche' l'EFIS deve poter mostrare una caduta vera, non un plateau.
[[nodiscard]] double lift_coefficient(const AeroCoefficients& aero,
                                      double alpha_rad) noexcept;

// Coefficiente di portanza massimo della cellula pulita, cioe' il valore che la
// retta raggiunge all'incidenza di stallo. Il modello oltre lo stallo e'
// costruito perche' questo resti il massimo effettivo: la velocita' di stallo
// calcolata e quella a cui l'aeroplano stalla sono quindi la stessa cosa.
[[nodiscard]] double max_lift_coefficient(const AeroCoefficients& aero) noexcept;

[[nodiscard]] AeroLoads aerodynamic_loads(const Airframe& airframe,
                                          const AeroState& state,
                                          const Controls& controls) noexcept;

// Velocita' di stallo in velocita' calibrata, alla massa e configurazione date.
// Ricavata dal modello e non dichiarata: e' cosi' che si verifica che le
// velocita' sul nastro descrivano la cellula che vola davvero.
[[nodiscard]] double stall_speed_kcas(const Airframe& airframe,
                                      double mass_kg,
                                      double flap_deg) noexcept;

// Incremento di portanza, resistenza e momento della tacca di flap attiva.
// Interpolato fra le tacche dichiarate: il flap vero si muove, e un motore che
// saltasse da una tacca all'altra produrrebbe uno scalino di accelerazione che
// l'EFIS mostrerebbe come un transitorio inesistente.
struct FlapEffect final {
    double delta_lift{};
    double delta_drag{};
    double delta_pitch{};
    double limit_kcas{};
};

[[nodiscard]] FlapEffect flap_effect(const Airframe& airframe,
                                     double flap_deg) noexcept;

}  // namespace efis::fdm
