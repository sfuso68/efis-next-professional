#pragma once

namespace efis::engine {

struct RuntimeConfig {
    double engine_run_threshold_rpm{500.0};
    double tach_reference_rpm{5000.0};
};

struct RuntimeTotals {
    double master_hours{};
    double engine_hours{};
    double tach_hours{};
    double flight_hours{};
};

// Accumulatore indipendente dall'interfaccia grafica. I quattro contatori
// restano distinti per non confondere alimentazione, funzionamento reale,
// tach-time ponderato e tempo di volo ricevuto dalla catena avionica.
class RuntimeTracker {
public:
    explicit RuntimeTracker(RuntimeConfig config = {}, RuntimeTotals initial = {});

    void update(double rpm, double elapsed_seconds, bool airborne);
    [[nodiscard]] const RuntimeTotals& totals() const noexcept { return totals_; }
    [[nodiscard]] bool engine_running() const noexcept { return engine_running_; }

private:
    RuntimeConfig config_;
    RuntimeTotals totals_;
    bool engine_running_{};
};

}  // namespace efis::engine
