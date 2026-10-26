#include "efis/fdm/atmosphere.hpp"

#include <algorithm>
#include <cmath>

namespace efis::fdm {
namespace {

// Esponente della relazione barometrica nella troposfera: g / (R L). Non e'
// scritto come numero perche' deve restare legato alle costanti da cui
// discende: cambiare il gradiente e dimenticare l'esponente produrrebbe un
// altimetro sbagliato in modo difficile da vedere.
const double troposphere_exponent = standard_gravity_m_s2 /
                                    (dry_air_gas_constant_j_kg_k * isa_lapse_rate_k_m);

// Esponente della stessa relazione scritta per la densita' invece che per la
// pressione. Serve alla quota di densita'.
const double density_exponent = troposphere_exponent - 1.0;

// Pressione standard alla tropopausa, dalla quale riparte l'andamento
// esponenziale dello strato isotermo.
const double tropopause_pressure_ratio =
    std::pow(isa_tropopause_temperature_k / isa_sea_level_temperature_k,
             troposphere_exponent);

// Velocita' del suono al livello del mare standard. E' la costante che compare
// nella definizione di velocita' calibrata, e per questo non e' la velocita'
// del suono del giorno: la CAS e' definita sull'atmosfera standard, non su
// quella vera.
const double sea_level_speed_of_sound_m_s =
    std::sqrt(dry_air_gamma * dry_air_gas_constant_j_kg_k * isa_sea_level_temperature_k);

// Temperatura della ISA pura alla quota data, senza scostamenti.
[[nodiscard]] double isa_temperature_k(double altitude_m) noexcept {
    if (altitude_m <= isa_tropopause_altitude_m) {
        return isa_sea_level_temperature_k - isa_lapse_rate_k_m * altitude_m;
    }
    return isa_tropopause_temperature_k;
}

// Rapporto fra la pressione alla quota data e quella al livello del mare, nella
// ISA pura.
//
// Lo scostamento di temperatura non entra qui, ed e' una scelta, non una
// semplificazione dimenticata: l'altimetro misura pressione e la converte con
// la ISA, quindi in una giornata fredda la quota indicata resta quella che la
// ISA associa a quella pressione. Se il profilo di pressione seguisse la
// temperatura vera, la quota indicata dal banco non sarebbe piu' quella che
// legge uno strumento reale, e l'errore di temperatura -- che e' esattamente il
// fenomeno da riprodurre -- sparirebbe dal modello.
[[nodiscard]] double isa_pressure_ratio(double altitude_m) noexcept {
    if (altitude_m <= isa_tropopause_altitude_m) {
        const double temperature_ratio = isa_temperature_k(altitude_m) /
                                         isa_sea_level_temperature_k;
        return std::pow(std::max(temperature_ratio, 1.0e-6), troposphere_exponent);
    }
    const double isothermal = std::exp(-standard_gravity_m_s2 *
                                       (altitude_m - isa_tropopause_altitude_m) /
                                       (dry_air_gas_constant_j_kg_k *
                                        isa_tropopause_temperature_k));
    return tropopause_pressure_ratio * isothermal;
}

}  // namespace

AirState Atmosphere::at_altitude(double altitude_m) const noexcept {
    AirState air{};
    air.pressure_pa = sea_level_pressure_pa * isa_pressure_ratio(altitude_m);
    air.temperature_k = std::max(isa_temperature_k(altitude_m) + temperature_offset_k,
                                 1.0);
    air.density_kg_m3 = air.pressure_pa /
                        (dry_air_gas_constant_j_kg_k * air.temperature_k);
    air.speed_of_sound_m_s = std::sqrt(dry_air_gamma * dry_air_gas_constant_j_kg_k *
                                       air.temperature_k);
    return air;
}

double Atmosphere::pressure_altitude_m(double pressure_pa) noexcept {
    const double ratio = std::max(pressure_pa, 1.0) / isa_sea_level_pressure_pa;
    if (ratio >= tropopause_pressure_ratio) {
        const double temperature_ratio = std::pow(ratio, 1.0 / troposphere_exponent);
        return (isa_sea_level_temperature_k / isa_lapse_rate_k_m) *
               (1.0 - temperature_ratio);
    }
    return isa_tropopause_altitude_m -
           (dry_air_gas_constant_j_kg_k * isa_tropopause_temperature_k /
            standard_gravity_m_s2) *
               std::log(ratio / tropopause_pressure_ratio);
}

double Atmosphere::indicated_altitude_m(double pressure_pa,
                                        double baro_setting_pa) noexcept {
    // La differenza fra due quote di pressione, non una correzione lineare.
    // I 27 piedi per ettopascal valgono al livello del mare e non in quota, e
    // la forma per differenza li produce senza che nessuno li scriva.
    return pressure_altitude_m(pressure_pa) - pressure_altitude_m(baro_setting_pa);
}

double Atmosphere::density_altitude_m(double altitude_m) const noexcept {
    const AirState air = at_altitude(altitude_m);
    const double ratio = air.density_kg_m3 / isa_sea_level_density_kg_m3;
    return (isa_sea_level_temperature_k / isa_lapse_rate_k_m) *
           (1.0 - std::pow(std::max(ratio, 1.0e-6), 1.0 / density_exponent));
}

double impact_pressure_from_cas_pa(double cas_m_s) noexcept {
    const double mach = std::max(cas_m_s, 0.0) / sea_level_speed_of_sound_m_s;
    const double factor = 1.0 + 0.2 * mach * mach;
    return isa_sea_level_pressure_pa * (std::pow(factor, 3.5) - 1.0);
}

double cas_from_impact_pressure_m_s(double impact_pressure_pa) noexcept {
    const double qc = std::max(impact_pressure_pa, 0.0);
    const double factor = std::pow(qc / isa_sea_level_pressure_pa + 1.0, 2.0 / 7.0) - 1.0;
    return sea_level_speed_of_sound_m_s * std::sqrt(std::max(5.0 * factor, 0.0));
}

double tas_from_cas_m_s(double cas_m_s, const AirState& air) noexcept {
    const double qc = impact_pressure_from_cas_pa(cas_m_s);
    const double static_pressure = std::max(air.pressure_pa, 1.0);
    const double factor = std::pow(qc / static_pressure + 1.0, 2.0 / 7.0) - 1.0;
    const double mach = std::sqrt(std::max(5.0 * factor, 0.0));
    return mach * air.speed_of_sound_m_s;
}

double cas_from_tas_m_s(double tas_m_s, const AirState& air) noexcept {
    const double mach = std::max(tas_m_s, 0.0) / air.speed_of_sound_m_s;
    const double factor = 1.0 + 0.2 * mach * mach;
    const double qc = std::max(air.pressure_pa, 1.0) * (std::pow(factor, 3.5) - 1.0);
    return cas_from_impact_pressure_m_s(qc);
}

double mach_from_tas(double tas_m_s, const AirState& air) noexcept {
    return tas_m_s / std::max(air.speed_of_sound_m_s, 1.0);
}

double dynamic_pressure_pa(double tas_m_s, const AirState& air) noexcept {
    return 0.5 * air.density_kg_m3 * tas_m_s * tas_m_s;
}

}  // namespace efis::fdm
