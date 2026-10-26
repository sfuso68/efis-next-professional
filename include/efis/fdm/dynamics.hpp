#pragma once

#include "efis/fdm/airframe.hpp"
#include "efis/fdm/atmosphere.hpp"
#include "efis/fdm/propulsion.hpp"

namespace efis::fdm {

// Dinamica del volo a sei gradi di liberta'.
//
// Perche' sei gradi e non una traiettoria scritta a mano. Il simulatore di
// sistema che il banco possiede gia' percorre un copione: a un certo istante la
// quota vale tanto e la velocita' tanto. Serve, e resta, perche' una
// regressione deve essere ripetibile al campione. Ma un copione non puo'
// produrre le grandezze che l'EFIS deve confrontare fra loro, perche' non le
// lega: velocita' indicata, velocita' al suolo, rateo di salita barometrico e
// satellitare, rateo di virata, accelerazione laterale e fattore di carico sono
// tutte conseguenze dello stesso moto. Scritte a mano una per una possono
// essere coerenti solo per caso, e un confronto incrociato tarato su dati
// incoerenti per costruzione non significa nulla. Integrare il moto e' il solo
// modo di ottenerle tutte vere insieme.
//
// Limiti dichiarati, perche' si sappiano prima di incontrarli:
//
// - assetto in angoli di Eulero. Oltre 85 gradi di beccheggio la relazione fra
//   ratei e angoli degenera. Un acrobatico non si simula qui;
// - terra piatta a quota di campo costante, con gravita' costante. Le
//   coordinate si aggiornano sul raggio terrestre locale, quindi a lungo raggio
//   la rotta e' buona e la quota del terreno no;
// - prodotto d'inerzia trascurato: la matrice e' diagonale. Su cellule con ala
//   molto a freccia introduce un accoppiamento rollio-imbardata che qui non
//   c'e', e nessuna delle due cellule di riferimento ha ala a freccia;
// - nessun effetto di scia dell'elica sul piano di coda, nessuna coppia di
//   reazione. L'aeroplano non imbarda dando motore, e un vero monomotore lo
//   fa.

// Vento e turbolenza.
//
// La direzione e' quella da cui il vento spira, come nei bollettini e come la
// mostra l'EFIS: non e' una convenzione comoda, e' quella che il pilota legge.
struct Wind final {
    double from_deg{};
    double speed_kt{};

    // Ampiezza della raffica, in nodi. Il banco la applica come variazione
    // lenta, non come rumore bianco: una raffica e' un fenomeno di secondi, e
    // il filtro di un calcolatore dati aria la segue, mentre seguirebbe un
    // rumore bianco solo mediandolo via.
    double gust_kt{};
    double updraft_m_s{};
};

// Ambiente in cui si svolge il volo.
struct Environment final {
    Atmosphere atmosphere{};
    Wind wind{};

    // Quota del campo sul livello medio del mare. Serve al modello di
    // contatto: il carrello tocca a questa quota, non a zero.
    double field_elevation_m{};

    // Declinazione magnetica del luogo. Senza di essa la prua magnetica
    // coincide con quella vera, e un EFIS tarato cosi' sbaglierebbe la rotta di
    // qualche grado a ogni volo. In Italia vale circa tre gradi est.
    double magnetic_variation_deg{3.0};
};

// Stato del veicolo. E' la verita' del modello: cio' che l'aeroplano fa
// davvero, prima che un sensore lo misuri e lo sporchi. La distinzione fra
// questo e cio' che l'EFIS riceve e' il motivo per cui il banco esiste: senza
// una verita' nota non si puo' dire se lo strumento sbaglia.
struct VehicleState final {
    double latitude_deg{};
    double longitude_deg{};
    double altitude_m{};

    // Velocita' rispetto al suolo in assi corpo.
    double body_u_m_s{};
    double body_v_m_s{};
    double body_w_m_s{};

    double roll_rad{};
    double pitch_rad{};
    double heading_rad{};

    double roll_rate_rad_s{};
    double pitch_rate_rad_s{};
    double yaw_rate_rad_s{};

    double mass_kg{};
    double fuel_kg{};
    bool on_ground{true};

    // --- Grandezze derivate ------------------------------------------------
    //
    // Ricalcolate a ogni passo e non integrate: derivano dallo stato, e tenerle
    // come stato separato significherebbe poterle far divergere da esso.
    double true_airspeed_m_s{};
    double calibrated_airspeed_m_s{};
    double ground_speed_m_s{};
    double track_rad{};
    double alpha_rad{};
    double sideslip_rad{};
    double vertical_speed_m_s{};
    double mach{};

    // Accelerazioni specifiche misurate in assi corpo: cio' che un
    // accelerometro legge, gravita' esclusa. Il fattore di carico e
    // l'accelerazione laterale della pallina discendono da queste, e non da una
    // formula di virata corretta: un'accelerazione laterale calcolata dalla
    // formula sarebbe sempre zero, e la pallina non si muoverebbe mai.
    double accel_x_g{};
    double accel_y_g{};
    double accel_z_g{};
    double load_factor{1.0};

    bool stalled{};
    double lift_coefficient{};

    // Pressioni che i sensori leggerebbero in questo punto. Stanno nella
    // verita' e non nello strato sensori perche' sono fisica dell'aria, non
    // comportamento di uno strumento: lo strato sensori le prende e vi aggiunge
    // errore, ritardo e guasto.
    double static_pressure_pa{};
    double impact_pressure_pa{};
    double outside_air_temperature_c{};
};

class Vehicle final {
public:
    Vehicle(Airframe airframe, Environment environment);

    // Pone l'aeroplano fermo sul campo, a motore spento. E' lo stato cold and
    // dark: il banco deve poter partire da li' perche' l'accensione e la corsa
    // di decollo sono due delle sequenze che l'EFIS mostra piu' spesso e che
    // nessun avvio in volo esercita.
    void place_on_ground(double latitude_deg, double longitude_deg,
                         double heading_deg);

    // Pone l'aeroplano in volo livellato alla quota e alla velocita' calibrata
    // date, con assetto, equilibratore e manetta stimati per l'equilibrio.
    // L'equilibrio non e' esatto: l'aeroplano si assesta nei primi secondi,
    // perche' la stima e' statica e il modello e' dinamico. Dichiararlo e'
    // meglio che inseguire un trim perfetto che la prima raffica rompe.
    void place_in_flight(double latitude_deg, double longitude_deg,
                         double altitude_ft, double heading_deg,
                         double calibrated_airspeed_kt);

    void set_controls(const Controls& controls) noexcept { controls_ = controls; }
    [[nodiscard]] const Controls& controls() const noexcept { return controls_; }
    [[nodiscard]] Controls& mutable_controls() noexcept { return controls_; }

    void set_environment(const Environment& environment) noexcept {
        environment_ = environment;
    }
    [[nodiscard]] const Environment& environment() const noexcept {
        return environment_;
    }

    // Avanza di dt secondi. Il passo richiesto viene suddiviso internamente:
    // la dinamica rotazionale di un aeroplano leggero ha costanti di tempo di
    // frazioni di secondo, e integrarla con il passo di pubblicazione del
    // display produrrebbe divergenze a venti hertz. Il chiamante chiede il
    // passo che gli serve per pubblicare; il modello decide quello che gli
    // serve per restare stabile.
    void step(double dt_s);

    [[nodiscard]] const VehicleState& state() const noexcept { return state_; }
    [[nodiscard]] const PropulsionState& engine() const noexcept {
        return propulsion_.state();
    }
    [[nodiscard]] Propulsion& propulsion() noexcept { return propulsion_; }
    [[nodiscard]] const Propulsion& propulsion() const noexcept { return propulsion_; }
    [[nodiscard]] const Airframe& airframe() const noexcept { return airframe_; }

    // Tempo di simulazione trascorso. Serve a chi registra: la verita' del
    // banco e' ripetibile, e un istante ripetibile deve avere un nome.
    [[nodiscard]] double elapsed_s() const noexcept { return elapsed_s_; }

private:
    void integrate(double dt_s);
    void update_derived(const AirState& air);
    [[nodiscard]] AirState air_at_current_altitude() const;

    struct GroundLoads final {
        double force_x_n{};
        double force_y_n{};
        double force_z_n{};
        double moment_roll_nm{};
        double moment_pitch_nm{};
        double moment_yaw_nm{};
        bool in_contact{};
    };

    [[nodiscard]] GroundLoads ground_loads(const AirState& air) const;

    Airframe airframe_;
    Environment environment_;
    Propulsion propulsion_;
    Controls controls_{};
    VehicleState state_{};
    double elapsed_s_{};

    // Fase della raffica, avanzata a ogni passo. Una raffica deterministica:
    // il banco deve essere ripetibile, e un vento casuale renderebbe due
    // esecuzioni della stessa prova diverse.
    double gust_phase_{};
};

}  // namespace efis::fdm
