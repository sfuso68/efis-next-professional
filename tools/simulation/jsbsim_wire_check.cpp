// Offline SIM adapter/codec check. No sockets, destinations, or production node identity.
#include "efis/core/freshness_budget.hpp"
#include "efis/core/signal_access.hpp"
#include "efis/protocol/ddp_session.hpp"
#include "efis/protocol/edp_v1.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace efis;
using namespace std::chrono_literals;
constexpr std::uint32_t sim_node = 65000; // Deliberately NOT the production Blackbox node 47.
constexpr std::uint64_t instance = 11415;

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

Sample<double> convert(const QJsonObject& group, const char* key, const char* unit, SignalId signal,
                       Unit target_unit, MonoTime now, std::uint64_t seq) {
    require(group.contains(key) && group[key].isObject(), "missing sample");
    const auto input = group[key].toObject();
    require(input["simulated"].isBool() && input["simulated"].toBool(), "sample not SIM");
    require(input["unit"].toString() == unit && !input["property"].toString().isEmpty(),
            "sample unit/provenance mismatch");
    Sample<double> out;
    out.source = SourceId::simulation;
    out.sampled_at = out.received_at = now;
    out.sequence = seq;
    out.origin = {
        signal, static_cast<NodeId>(sim_node), static_cast<ChannelId>(signal), target_unit, 0,
        true};
    const auto validity = input["validity"].toString();
    if (validity == "UNAVAILABLE") {
        require(input["value"].isNull(), "unavailable sample has value");
        out.validity = Validity::unavailable;
        return out;
    }
    require(validity == "SIMULATED" && input["value"].isDouble() && input["raw"].isDouble(),
            "sample invalid status/type");
    out.value = input["value"].toDouble();
    out.raw = input["raw"].toDouble();
    require(std::isfinite(out.value) && std::isfinite(out.raw), "nonfinite sample");
    out.validity = Validity::simulated;
    out.quality = Quality::nominal;
    return out;
}

struct Adapted {
    FlightSnapshot flight;
    engine::EngineSnapshot engine;
    MonoTime now;
};

Adapted adapt(const QJsonObject& input, std::uint64_t index) {
    require(input["schema"].toString() == "fuso-avionics.physics-sil.v1", "schema mismatch");
    require(input["simulated"].isBool() && input["simulated"].toBool() &&
                input["flight_approval"].isBool() && !input["flight_approval"].toBool(),
            "frame not SIM");
    require(input["profile_status"].toString() == "REFERENCE_UNVALIDATED" &&
                input["backend"].toString() == "JSBSim-1.3.3" &&
                input["model"].toString() == "c172p",
            "unknown model/backend");
    require(input["sequence"].isDouble() &&
                input["sequence"].toDouble() == static_cast<double>(index) &&
                input["sim_time_ms"].isDouble() &&
                input["sim_time_ms"].toDouble() == static_cast<double>(index * 50),
            "frame sequence/clock mismatch");
    Adapted a;
    a.now = MonoTime{} + 10s + std::chrono::milliseconds(index * 50);
    a.flight.timestamp = a.engine.timestamp = a.now;
    a.flight.sequence = a.engine.sequence = index + 1;
    // Missing domain mappings are intentionally unavailable. Never alias CAS to IAS,
    // geometric MSL to barometric altitude, or TRUE heading to an unstated heading convention.
    const auto flight = input["flight"].toObject();
    a.flight.attitude.roll_deg =
        convert(flight, "roll_deg", "deg", SignalId::roll, Unit::degree, a.now, index + 1);
    a.flight.attitude.pitch_deg =
        convert(flight, "pitch_deg", "deg", SignalId::pitch, Unit::degree, a.now, index + 1);
    a.flight.position.latitude_deg =
        convert(flight, "latitude_deg", "deg", SignalId::latitude, Unit::degree, a.now, index + 1);
    a.flight.position.longitude_deg = convert(flight, "longitude_deg", "deg", SignalId::longitude,
                                              Unit::degree, a.now, index + 1);
    a.flight.position.ground_speed_kt =
        convert(flight, "gs_kt", "kt", SignalId::ground_speed, Unit::knot, a.now, index + 1);
    // No GNSS/baro independent sensor model exists yet: geometric VS is not duplicated into both.
    a.flight.health.level = HealthLevel::warning;
    for (const auto signal : flight_signals()) {
        const auto* datum = find_signal(a.flight, signal);
        if (datum && !datum->usable())
            ++a.flight.health.unavailable_fields;
    }
    const auto e = input["engine"].toObject();
    struct Mapping {
        Sample<double>* sample;
        const char* key;
        const char* unit;
        SignalId signal;
        Unit target;
    };
    const std::array mappings{
        Mapping{&a.engine.rpm, "rpm", "rpm", SignalId::engine_rpm, Unit::revolutions_per_minute},
        Mapping{&a.engine.manifold_pressure_inhg, "map_inhg", "inHg", SignalId::manifold_pressure,
                Unit::inch_mercury},
        Mapping{&a.engine.oil_pressure_psi, "oil_pressure_psi", "psi", SignalId::oil_pressure,
                Unit::pound_per_square_inch},
        Mapping{&a.engine.oil_temperature_c, "oil_temperature_c", "degC", SignalId::oil_temperature,
                Unit::celsius},
        Mapping{&a.engine.fuel_pressure_psi, "fuel_pressure_psi", "psi", SignalId::fuel_pressure,
                Unit::pound_per_square_inch},
        Mapping{&a.engine.fuel_flow_gph, "flow_gph", "USgal/h", SignalId::fuel_flow,
                Unit::gallon_per_hour},
        Mapping{&a.engine.fuel_left_gal, "left_gal", "USgal", SignalId::fuel_quantity_left,
                Unit::gallon},
        Mapping{&a.engine.fuel_right_gal, "right_gal", "USgal", SignalId::fuel_quantity_right,
                Unit::gallon},
        Mapping{&a.engine.bus_voltage_v, "bus_v", "V", SignalId::bus_voltage, Unit::volt},
        Mapping{&a.engine.alternator_current_a, "current_a", "A", SignalId::alternator_current,
                Unit::ampere}};
    for (const auto& m : mappings)
        *m.sample = convert(e, m.key, m.unit, m.signal, m.target, a.now, index + 1);
    for (std::size_t i = 0; i < 4; ++i) {
        const auto cht = "cht" + std::to_string(i + 1), egt = "egt" + std::to_string(i + 1);
        a.engine.cht_c[i] = convert(
            e, cht.c_str(), "degC",
            static_cast<SignalId>(static_cast<unsigned>(SignalId::cylinder_head_temperature_1) + i),
            Unit::celsius, a.now, index + 1);
        a.engine.egt_c[i] = convert(
            e, egt.c_str(), "degC",
            static_cast<SignalId>(static_cast<unsigned>(SignalId::exhaust_gas_temperature_1) + i),
            Unit::celsius, a.now, index + 1);
    }
    a.engine.health = engine::EngineHealth::caution;
    return a;
}

int run(const QString& filename) {
    QFile file(filename);
    require(file.open(QIODevice::ReadOnly), "cannot open verified session frames");
    protocol::ddp::SnapshotPublisher dp{
        {protocol::ddp::Profile::professional, sim_node, instance, 1}};
    protocol::edp::Publisher ep{{sim_node, instance, 1}};
    protocol::ddp::SnapshotConsumer pfd{{250ms, 1000ms, sim_node}}, mfd{{250ms, 1000ms, sim_node}};
    protocol::edp::Consumer ems{{250ms, 1000ms, sim_node}};
    protocol::ddp::SnapshotConsumer production_pfd{{250ms, 1000ms, 47}};
    protocol::edp::Consumer production_ems{{250ms, 1000ms, 47}};
    std::uint64_t count = 0;
    QString session;
    MonoTime last{};
    while (!file.atEnd()) {
        const auto line = file.readLine(65537);
        require(line.size() <= 65536, "frame too large");
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(line, &error);
        require(error.error == QJsonParseError::NoError && doc.isObject(), "invalid frame JSON");
        const auto input = doc.object();
        require(!input["session_id"].toString().isEmpty(), "missing session ID");
        if (count == 0)
            session = input["session_id"].toString();
        require(session == input["session_id"].toString(), "session changed");
        const auto a = adapt(input, count);
        const auto d = dp.publish(a.flight, a.now), e = ep.publish(a.engine, a.now);
        require(static_cast<bool>(pfd.consume(d, a.now)) &&
                    static_cast<bool>(mfd.consume(d, a.now)),
                "DDP consumer rejected");
        require(ems.consume(e, a.now) == protocol::edp::DecodeError::none, "EDP consumer rejected");
        require(pfd.latest()->sequence == mfd.latest()->sequence &&
                    pfd.latest()->sequence == ems.latest()->sequence,
                "consumer frame skew");
        require(pfd.latest()->attitude.roll_deg.is_simulated() && ems.latest()->rpm.is_simulated(),
                "SIM provenance lost");
        require(!pfd.latest()->air_data.indicated_altitude_ft.usable() &&
                    !pfd.latest()->air_data.indicated_airspeed_kt.usable() &&
                    !pfd.latest()->attitude.heading_deg.usable(),
                "unapproved convention mapping");
        require(!ems.latest()->cht_c[0].usable() && ems.latest()->cht_c[0].is_simulated(),
                "fake cylinder data");
        require(std::abs(ems.latest()->rpm.value - a.engine.rpm.value) < .01, "RPM wire mismatch");
        if (count == 0) {
            require(!production_pfd.consume(d, a.now), "production node accepted SIM identity");
            require(production_ems.consume(e, a.now) != protocol::edp::DecodeError::none,
                    "production EMS accepted SIM identity");
            require(!pfd.consume(d, a.now), "DDP duplicate accepted");
            require(ems.consume(e, a.now) != protocol::edp::DecodeError::none,
                    "EDP duplicate accepted");
            auto corrupt_d = d, corrupt_e = e;
            corrupt_d.back() ^= 1;
            corrupt_e.back() ^= 1;
            require(!mfd.consume(corrupt_d, a.now), "DDP corrupt accepted");
            require(ems.consume(corrupt_e, a.now) != protocol::edp::DecodeError::none,
                    "EDP corrupt accepted");
        }
        last = a.now;
        ++count;
        require(count <= 72001, "too many frames");
    }
    require(count >= 21, "session too short");
    require(pfd.state(last + 251ms) == protocol::ddp::LinkState::stale &&
                mfd.state(last + 251ms) == protocol::ddp::LinkState::stale &&
                ems.state(last + 251ms) == protocol::edp::LinkState::stale,
            "stale timeout mismatch");
    require(pfd.state(last + 1001ms) == protocol::ddp::LinkState::lost &&
                mfd.state(last + 1001ms) == protocol::ddp::LinkState::lost &&
                ems.state(last + 1001ms) == protocol::edp::LinkState::lost,
            "lost timeout mismatch");
    std::cout << "SIL_CANONICAL_CONSUMERS=PASS frames=" << count
              << " source_node=65000 PFD_MFD_DDP=PASS EMS_EDP=PASS SIM=PASS CRC_DUP_ID_TIMEOUT=PASS"
                 " AIRDATA_HEADING_MAPPING=UNAVAILABLE SOCKETS=NONE flight_approval=false\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "usage: efis-sil-wire-check VERIFIED_SESSION_FRAMES.jsonl");
        return run(QString::fromLocal8Bit(argv[1]));
    } catch (const std::exception& e) {
        std::cerr << "SIL_CANONICAL_CONSUMERS=FAIL reason=" << e.what() << '\n';
        return 2;
    }
}
