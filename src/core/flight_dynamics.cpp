#include "efis/core/flight_dynamics.hpp"

#include <cmath>

namespace efis {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double degrees_per_radian = 180.0 / pi;
}  // namespace

double coordinated_turn_rate_deg_s(double bank_deg, double true_airspeed_kt) noexcept {
    const double speed = true_airspeed_kt * metres_per_second_per_knot;
    // Un aeromobile fermo e inclinato non sta virando. Dividere per zero
    // produrrebbe un rateo infinito, che il quadrante mostrerebbe a fondo
    // scala: una virata violenta al posto di un aeroplano parcheggiato.
    if (speed <= 0.0) return 0.0;

    const double bank = bank_deg / degrees_per_radian;
    return standard_gravity_m_s2 * std::tan(bank) / speed * degrees_per_radian;
}

double bank_for_turn_rate_deg(double turn_rate_deg_s, double true_airspeed_kt) noexcept {
    const double speed = true_airspeed_kt * metres_per_second_per_knot;
    if (speed <= 0.0) return 0.0;

    const double rate = turn_rate_deg_s / degrees_per_radian;
    return std::atan(rate * speed / standard_gravity_m_s2) * degrees_per_radian;
}

}  // namespace efis
