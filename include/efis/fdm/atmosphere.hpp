#pragma once

#include "efis/core/flight_dynamics.hpp"

namespace efis::fdm {

// Atmosfera e relazioni dei dati aria.
//
// Sta alla base di tutto il motore di volo e non e' un dettaglio del
// simulatore: sono le stesse relazioni che l'EFIS deve invertire per ricavare
// quota e velocita' dalle pressioni misurate. Averle in un posto solo, scritte
// nella forma canonica, significa che il banco e lo strumento non possono
// divergere per una costante diversa.
//
// Il modello e' l'atmosfera standard internazionale (ISA) della troposfera e
// della bassa stratosfera, con due scostamenti che in volo vero esistono sempre
// e che il banco deve poter riprodurre: la pressione al livello del mare
// diversa da 1013,25 hPa e la temperatura diversa da quella standard. Senza il
// primo non si puo' provare la regolazione barometrica; senza il secondo non si
// puo' provare la differenza fra quota di pressione e quota vera, che e' la
// ragione per cui un aeroplano si schianta contro una montagna in giornata
// fredda.

// --- Costanti ISA ------------------------------------------------------------
//
// Valori della definizione ISO 2533, non arrotondamenti comodi: la quota di
// pressione calcolata con 0,0065 K/m invece di 0,0019812 K/ft deve dare lo
// stesso numero dell'altimetro, e la differenza si vede nei decimi di piede.

inline constexpr double isa_sea_level_pressure_pa = 101325.0;
inline constexpr double isa_sea_level_temperature_k = 288.15;
inline constexpr double isa_sea_level_density_kg_m3 = 1.225;
inline constexpr double isa_lapse_rate_k_m = 0.0065;
inline constexpr double isa_tropopause_altitude_m = 11000.0;
inline constexpr double isa_tropopause_temperature_k = 216.65;
inline constexpr double dry_air_gas_constant_j_kg_k = 287.05287;
inline constexpr double dry_air_gamma = 1.4;

// Gravita' e conversione dei nodi arrivano dal nucleo invece di essere
// ridefinite qui.
//
// La ragione non e' l'economia di due righe: due definizioni della stessa
// costante possono divergere, e il giorno in cui divergono il banco e il
// confronto incrociato del nucleo calcolerebbero il rateo di virata della
// stessa virata in modo diverso. Un disaccordo fra le due catene e'
// esattamente cio' che ADR-004 dichiara essere un guasto, e sarebbe prodotto
// da noi.
using efis::metres_per_second_per_knot;
using efis::standard_gravity_m_s2;

// --- Conversioni -------------------------------------------------------------
//
// Dichiarate qui perche' il motore lavora in SI e l'avionica in nodi e piedi, e
// la conversione sbagliata e' il modo piu' comune di sbagliare un numero
// altrimenti giusto.

inline constexpr double metres_per_foot = 0.3048;
inline constexpr double feet_per_metre = 1.0 / metres_per_foot;
inline constexpr double knots_per_metre_per_second = 1.0 / metres_per_second_per_knot;
inline constexpr double pascal_per_hectopascal = 100.0;
inline constexpr double pascal_per_inch_mercury = 3386.389;
inline constexpr double feet_per_minute_per_metre_per_second = feet_per_metre * 60.0;

// Condizione dell'aria in un punto. Le tre grandezze non sono indipendenti, ma
// restituirle insieme evita che il chiamante ricalcoli la densita' da pressione
// e temperatura con una costante diversa da quella usata qui.
struct AirState final {
    double pressure_pa{isa_sea_level_pressure_pa};
    double temperature_k{isa_sea_level_temperature_k};
    double density_kg_m3{isa_sea_level_density_kg_m3};
    double speed_of_sound_m_s{340.294};
};

// Atmosfera del banco: la ISA piu' i due scostamenti che contano.
//
// La pressione al livello del mare e' quella vera del giorno, non la
// regolazione dell'altimetro: sono due cose diverse e confonderle e' proprio
// l'errore che il banco deve permettere di provare. Il pilota regola la
// seconda; la prima la decide il tempo.
struct Atmosphere final {
    double sea_level_pressure_pa{isa_sea_level_pressure_pa};

    // Scostamento della temperatura dalla standard, costante con la quota.
    // E' la forma in cui il dato arriva da un METAR o da un'analisi: ISA+15,
    // ISA-10. Il gradiente resta quello standard, perche' un gradiente diverso
    // richiederebbe un profilo misurato che il banco non ha.
    double temperature_offset_k{0.0};

    [[nodiscard]] AirState at_altitude(double altitude_m) const noexcept;

    // Quota geopotenziale a cui la ISA pura ha la pressione data. E' cio' che
    // misura un altimetro regolato a 1013,25: la quota di pressione, l'unica
    // confrontabile fra aeromobili diversi e la sola che va nella black box.
    [[nodiscard]] static double pressure_altitude_m(double pressure_pa) noexcept;

    // Quota indicata da un altimetro con la regolazione data. Coincide con la
    // quota di pressione solo a regolazione standard, e la differenza non e'
    // una curiosita': sono i circa 27 piedi per ettopascal che separano la
    // quota letta da quella vera quando la regolazione e' sbagliata.
    [[nodiscard]] static double indicated_altitude_m(double pressure_pa,
                                                     double baro_setting_pa) noexcept;

    // Quota a cui la ISA pura ha la densita' che qui si ha davvero. Decide le
    // prestazioni, non la separazione: e' la quota che l'aeroplano "sente" al
    // decollo in una giornata calda.
    [[nodiscard]] double density_altitude_m(double altitude_m) const noexcept;
};

// --- Relazioni del tubo di Pitot --------------------------------------------
//
// Compressibili, non incomprimibili. La forma 0,5 rho V^2 e' sbagliata di circa
// un nodo gia' a 200 kt al livello del mare e di piu' in quota, e un EFIS che
// la usasse mostrerebbe una velocita' che non e' quella del suo stesso tubo.
// Il banco deve generare la pressione d'impatto con la stessa relazione che lo
// strumento invertira', altrimenti si tara contro un errore proprio.

// Pressione d'impatto (qc = pressione totale meno statica) prodotta da una
// velocita' calibrata. Subsonica: la forma di Saint-Venant con la pressione
// statica al livello del mare, che e' la definizione stessa di CAS.
[[nodiscard]] double impact_pressure_from_cas_pa(double cas_m_s) noexcept;

// Inversa della precedente: la velocita' calibrata che l'anemometro mostra a
// fronte di una pressione d'impatto misurata. E' la relazione che gira nel
// calcolatore dati aria dell'EFIS.
[[nodiscard]] double cas_from_impact_pressure_m_s(double impact_pressure_pa) noexcept;

// Velocita' vera dalla calibrata, data la condizione dell'aria. La forma usata
// e' quella compressibile completa: il rapporto delle sole densita' e'
// un'approssimazione che sbaglia di qualche nodo in crociera alta.
[[nodiscard]] double tas_from_cas_m_s(double cas_m_s, const AirState& air) noexcept;

// Velocita' calibrata dalla vera. Serve al motore, che conosce la velocita'
// vera perche' integra la dinamica, e deve produrre cio' che il tubo misura.
[[nodiscard]] double cas_from_tas_m_s(double tas_m_s, const AirState& air) noexcept;

[[nodiscard]] double mach_from_tas(double tas_m_s, const AirState& air) noexcept;

// Pressione dinamica vera, quella che compare nelle forze aerodinamiche. Non e'
// la pressione d'impatto: questa e' incomprimibile per definizione perche' e'
// il fattore di scala delle forze, e usare l'una al posto dell'altra confonde
// la fisica con la strumentazione.
[[nodiscard]] double dynamic_pressure_pa(double tas_m_s, const AirState& air) noexcept;

}  // namespace efis::fdm
