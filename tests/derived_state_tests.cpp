#include "efis/core/derived_state.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

efis::Sample<double> sample(double value, efis::Validity validity,
                            bool simulated = false) {
    efis::Sample<double> out{};
    out.value = value;
    out.validity = validity;
    out.origin.simulated = simulated;
    return out;
}
}  // namespace

int main() {
    using namespace std::chrono_literals;
    const auto epoch = efis::MonoTime{};

    const auto invalid = efis::engine_integration_input(
        sample(5200.0, efis::Validity::unavailable),
        sample(5.4, efis::Validity::stale), 60.0);
    require(!invalid.runtime_usable && invalid.accountable_rpm == 0.0,
            "unavailable RPM must not accrue engine runtime");
    require(!invalid.fuel_usable && invalid.fuel_consumed_gallons == 0.0,
            "invalid RPM or fuel flow must not accrue fuel consumption");

    const auto live = efis::engine_integration_input(
        sample(5200.0, efis::Validity::valid),
        sample(6.0, efis::Validity::valid), 60.0);
    require(live.runtime_usable && live.accountable_rpm == 5200.0,
            "valid RPM must remain accountable");
    require(live.fuel_usable && std::abs(live.fuel_consumed_gallons - 0.1) < 1e-12,
            "valid fuel integration must remain correct");

    double previous = 100.0;
    double trend = 12.0;
    bool initialized = true;
    require(!efis::update_airspeed_trend(
                sample(0.0, efis::Validity::unavailable), previous, trend, initialized),
            "invalid IAS must not update its trend");
    require(!initialized && trend == 0.0,
            "invalid IAS must reset the derivative chain and displayed trend");
    require(efis::update_airspeed_trend(
                sample(105.0, efis::Validity::valid), previous, trend, initialized),
            "first valid IAS must establish a baseline");
    require(trend == 0.0,
            "first valid IAS after a gap must not create a false trend spike");

    efis::PublicationFreshness freshness;
    freshness.observe(1'000, epoch + 1s);
    require(freshness.fresh(epoch + 3s, 3s),
            "publication must be fresh inside the monotonic budget");
    require(!freshness.fresh(epoch + 5s, 3s),
            "unchanged publication must become stale monotonically");
    freshness.observe(900, epoch + 5s);
    require(freshness.fresh(epoch + 5s, 3s),
            "a changed token remains a new publication even after a wall-clock step");

    std::cout << "Derived state tests passed\n";
    return EXIT_SUCCESS;
}
