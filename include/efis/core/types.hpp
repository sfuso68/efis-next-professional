#pragma once

#include <chrono>
#include <cstdint>

namespace efis {

using MonoClock = std::chrono::steady_clock;
using MonoTime = MonoClock::time_point;

enum class SourceId : std::uint16_t {
    unknown,
    simulation,
    pixhawk_primary,
    esp32_legacy,
    dlvr_legacy,
    nav_rf,
    nav_synthetic,
    // Valore inserito dal pilota, non misurato da alcun sensore. La
    // provenienza va distinta perche' un dato inserito non invecchia come una
    // misura e non ha qualita' del segnale, ma puo' essere sbagliato: la
    // regolazione barometrica errata e' una causa classica di quota errata.
    // Aggiunta in coda: gli identificativi gia' registrati non si spostano.
    pilot_setting
};

enum class Quality : std::uint8_t { unknown, poor, nominal, high };

enum class Validity : std::uint8_t {
    valid,
    stale,
    unavailable,
    out_of_range,
    disagree,
    simulated,
    degraded
};

constexpr bool is_usable(Validity value) noexcept {
    return value == Validity::valid || value == Validity::simulated ||
           value == Validity::degraded;
}

// --- Identita' del campione --------------------------------------------------
//
// Un nodo e' una unita' fisica che produce dati: il Pixhawk, un modulo ESP32-S3,
// il nodo black box. Un canale e' l'ingresso concreto su quel nodo. Nessuno dei
// due e' un enum perche' entrambi sono configurabili dal Setup Center e non
// possono essere fissati nel codice. Il valore zero significa "non dichiarato",
// mai "il primo nodo".
using NodeId = std::uint16_t;
using ChannelId = std::uint16_t;

inline constexpr NodeId node_undeclared = 0;
inline constexpr ChannelId channel_undeclared = 0;

// Microsecondi dall'epoch UTC. Zero significa esplicitamente "UTC non
// affidabile": il nodo non dispone ancora di una sorgente di tempo valida, per
// esempio prima del fix GNSS. In quel caso l'unico riferimento utilizzabile e'
// il tempo monotono locale. Vedi paragrafo 17.2 dell'handoff, "UTC quando
// affidabile", e paragrafo 13: coordinate e quote senza fix sono invalide.
using UtcMicros = std::uint64_t;
inline constexpr UtcMicros utc_unavailable = 0;

// Identificativo stabile della grandezza fisica trasportata dal campione.
//
// Questi valori finiscono nella black box e nei log di volo, quindi devono
// restare stabili nel tempo: un identificativo che cambia significato rende
// illeggibili le registrazioni precedenti. Per questo ogni famiglia parte da una
// base esplicita e lascia spazio libero: un nuovo segnale si aggiunge in coda
// alla propria famiglia senza spostare nulla di gia' registrato.
enum class SignalId : std::uint16_t {
    unknown = 0,

    // Assetto
    roll = 100,
    pitch,
    heading,
    // Rateo di imbardata e accelerazione laterale: senza questi due il
    // virosbandometro e la pallina non hanno sorgente, e vanno misurati, non
    // ricavati derivando la prua.
    turn_rate,
    lateral_acceleration,

    // Dati aria
    indicated_airspeed = 200,
    pressure_altitude,
    vertical_speed,
    // Cio' di cui il sensore aria e' realmente portatore, oltre alla velocita'
    // che se ne ricava: la pressione differenziale misurata e la temperatura
    // dell'elemento, che serve a compensarla.
    differential_pressure,
    air_data_temperature,
    // Regolazione impostata sull'altimetro, e quota che ne risulta. Restano due
    // segnali distinti dalla quota di pressione: registrare solo quest'ultima
    // renderebbe incomparabili due voli condotti con regolazioni diverse.
    baro_setting,
    indicated_altitude,

    // Posizione
    latitude = 300,
    longitude,
    ground_speed,

    // Velocita' verticale ricavata dal satellitare, distinta da quella
    // barometrica: sono due catene fisicamente indipendenti, ed e' proprio la
    // loro indipendenza a renderle confrontabili.
    gnss_vertical_speed,

    // Guida di volo
    flight_director_active = 400,
    commanded_roll,
    commanded_pitch,
    selected_altitude,
    selected_course,
    selected_airspeed,

    // Navigazione
    cdi_deviation = 500,
    glide_slope_deviation,
    to_flag,

    // Motore
    engine_rpm = 600,
    manifold_pressure,
    oil_pressure,
    oil_temperature,
    fuel_pressure,
    fuel_flow,
    fuel_quantity_left,
    fuel_quantity_right,
    bus_voltage,
    alternator_current,
    cylinder_head_temperature_1 = 640,
    cylinder_head_temperature_2,
    cylinder_head_temperature_3,
    cylinder_head_temperature_4,
    exhaust_gas_temperature_1 = 660,
    exhaust_gas_temperature_2,
    exhaust_gas_temperature_3,
    exhaust_gas_temperature_4
};

// Unita' del valore ingegneristico. Dichiararla nel campione evita che una
// conversione sbagliata resti invisibile: un consumer che si aspetta nodi e
// riceve chilometri orari deve poterlo rilevare, non dedurlo dal nome del campo.
enum class Unit : std::uint8_t {
    unknown = 0,
    none,
    boolean,
    degree,
    knot,
    foot,
    foot_per_minute,
    dot,
    revolutions_per_minute,
    inch_mercury,
    pound_per_square_inch,
    celsius,
    gallon,
    gallon_per_hour,
    volt,
    ampere,
    // Aggiunte in coda: gli identificativi gia' presenti nelle registrazioni non
    // devono spostarsi.
    hectopascal,
    pascal,
    degree_per_second,
    // Accelerazione in multipli di quella di gravita', come la legge un
    // accelerometro e come la mostra la pallina.
    gravity
};

[[nodiscard]] constexpr const char* unit_symbol(Unit unit) noexcept {
    switch (unit) {
        case Unit::unknown: return "?";
        case Unit::none: return "";
        case Unit::boolean: return "bool";
        case Unit::degree: return "deg";
        case Unit::knot: return "kt";
        case Unit::foot: return "ft";
        case Unit::foot_per_minute: return "fpm";
        case Unit::dot: return "dot";
        case Unit::revolutions_per_minute: return "rpm";
        case Unit::inch_mercury: return "inHg";
        case Unit::pound_per_square_inch: return "psi";
        case Unit::celsius: return "C";
        case Unit::gallon: return "gal";
        case Unit::gallon_per_hour: return "gph";
        case Unit::volt: return "V";
        case Unit::ampere: return "A";
        case Unit::hectopascal: return "hPa";
        case Unit::pascal: return "Pa";
        case Unit::degree_per_second: return "deg/s";
        case Unit::gravity: return "G";
    }
    return "?";
}

// Nome stabile del segnale, pensato per le intestazioni della black box e per la
// diagnostica. Come per SignalId, cambiarlo invalida la lettura delle
// registrazioni precedenti.
[[nodiscard]] constexpr const char* signal_name(SignalId signal) noexcept {
    switch (signal) {
        case SignalId::unknown: return "unknown";
        case SignalId::roll: return "roll";
        case SignalId::pitch: return "pitch";
        case SignalId::heading: return "heading";
        case SignalId::turn_rate: return "turn_rate";
        case SignalId::lateral_acceleration: return "lateral_acceleration";
        case SignalId::indicated_airspeed: return "indicated_airspeed";
        case SignalId::pressure_altitude: return "pressure_altitude";
        case SignalId::vertical_speed: return "vertical_speed";
        case SignalId::differential_pressure: return "differential_pressure";
        case SignalId::air_data_temperature: return "air_data_temperature";
        case SignalId::baro_setting: return "baro_setting";
        case SignalId::indicated_altitude: return "indicated_altitude";
        case SignalId::latitude: return "latitude";
        case SignalId::longitude: return "longitude";
        case SignalId::ground_speed: return "ground_speed";
        case SignalId::gnss_vertical_speed: return "gnss_vertical_speed";
        case SignalId::flight_director_active: return "flight_director_active";
        case SignalId::commanded_roll: return "commanded_roll";
        case SignalId::commanded_pitch: return "commanded_pitch";
        case SignalId::selected_altitude: return "selected_altitude";
        case SignalId::selected_course: return "selected_course";
        case SignalId::selected_airspeed: return "selected_airspeed";
        case SignalId::cdi_deviation: return "cdi_deviation";
        case SignalId::glide_slope_deviation: return "glide_slope_deviation";
        case SignalId::to_flag: return "to_flag";
        case SignalId::engine_rpm: return "engine_rpm";
        case SignalId::manifold_pressure: return "manifold_pressure";
        case SignalId::oil_pressure: return "oil_pressure";
        case SignalId::oil_temperature: return "oil_temperature";
        case SignalId::fuel_pressure: return "fuel_pressure";
        case SignalId::fuel_flow: return "fuel_flow";
        case SignalId::fuel_quantity_left: return "fuel_quantity_left";
        case SignalId::fuel_quantity_right: return "fuel_quantity_right";
        case SignalId::bus_voltage: return "bus_voltage";
        case SignalId::alternator_current: return "alternator_current";
        case SignalId::cylinder_head_temperature_1: return "cht_1";
        case SignalId::cylinder_head_temperature_2: return "cht_2";
        case SignalId::cylinder_head_temperature_3: return "cht_3";
        case SignalId::cylinder_head_temperature_4: return "cht_4";
        case SignalId::exhaust_gas_temperature_1: return "egt_1";
        case SignalId::exhaust_gas_temperature_2: return "egt_2";
        case SignalId::exhaust_gas_temperature_3: return "egt_3";
        case SignalId::exhaust_gas_temperature_4: return "egt_4";
    }
    return "unknown";
}

}  // namespace efis
