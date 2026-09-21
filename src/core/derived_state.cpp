#include "efis/core/derived_state.hpp"

#include <algorithm>
#include <cmath>

namespace efis {

EngineIntegrationInput engine_integration_input(
    const Sample<double>& rpm, const Sample<double>& fuel_flow_gph,
    double elapsed_seconds) noexcept {
    EngineIntegrationInput input{};
    input.simulated = rpm.is_simulated() || fuel_flow_gph.is_simulated();
    input.runtime_usable = rpm.usable() && !rpm.is_simulated() &&
                           std::isfinite(rpm.value);
    input.accountable_rpm = input.runtime_usable ? rpm.value : 0.0;
    if (rpm.usable() && fuel_flow_gph.usable() &&
        std::isfinite(rpm.value) && std::isfinite(fuel_flow_gph.value) &&
        std::isfinite(elapsed_seconds) && rpm.value > 400.0 &&
        fuel_flow_gph.value > 0.0 && elapsed_seconds > 0.0) {
        input.fuel_consumed_gallons = fuel_flow_gph.value * elapsed_seconds / 3600.0;
        input.fuel_usable = true;
    }
    return input;
}

bool update_airspeed_trend(const Sample<double>& indicated_airspeed_kt,
                           double& previous_airspeed_kt,
                           double& filtered_projection_kt,
                           bool& initialized) noexcept {
    if (!indicated_airspeed_kt.usable() ||
        !std::isfinite(indicated_airspeed_kt.value)) {
        previous_airspeed_kt = 0.0;
        filtered_projection_kt = 0.0;
        initialized = false;
        return false;
    }
    if (initialized) {
        const double projection =
            (indicated_airspeed_kt.value - previous_airspeed_kt) * 300.0;
        filtered_projection_kt = filtered_projection_kt * 0.9 + projection * 0.1;
    } else {
        initialized = true;
    }
    previous_airspeed_kt = indicated_airspeed_kt.value;
    return true;
}

void PublicationFreshness::observe(std::uint64_t publication_token,
                                   MonoTime now) noexcept {
    if (!observed_ || publication_token != token_) {
        token_ = publication_token;
        observed_at_ = now;
        observed_ = true;
    }
}

void PublicationFreshness::reset() noexcept {
    token_ = 0;
    observed_at_ = {};
    observed_ = false;
}

bool PublicationFreshness::fresh(MonoTime now,
                                 std::chrono::milliseconds budget) const noexcept {
    return observed_ && now >= observed_at_ && now - observed_at_ <= budget;
}

}  // namespace efis
