#include "efis/protocol/ddp_v2.hpp"
#include "efis/protocol/edp_v1.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {
using namespace std::chrono_literals;

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    require(static_cast<bool>(input), "cannot open frozen vector JSON: " + path.string());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string vector_field(const std::string& json, const std::string& protocol,
                         const std::string& field) {
    const auto section = json.find('"' + protocol + '"');
    require(section != std::string::npos, "missing protocol in frozen vector JSON: " + protocol);
    const std::regex expression{"\\\"" + field + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\""};
    std::smatch match;
    const auto tail = json.substr(section);
    require(std::regex_search(tail, match, expression), "missing frozen field: " + field);
    return match[1].str();
}

std::size_t vector_size(const std::string& json, const std::string& protocol) {
    const auto section = json.find('"' + protocol + '"');
    require(section != std::string::npos, "missing protocol size section");
    const std::regex expression{"\\\"wire_size\\\"\\s*:\\s*([0-9]+)"};
    std::smatch match;
    const auto tail = json.substr(section);
    require(std::regex_search(tail, match, expression), "missing frozen wire_size");
    return static_cast<std::size_t>(std::stoull(match[1].str()));
}

constexpr std::uint32_t rotate_right(std::uint32_t value, unsigned count) {
    return (value >> count) | (value << (32U - count));
}

std::string sha256(const std::vector<std::uint8_t>& input) {
    static constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
    std::vector<std::uint8_t> data = input;
    const std::uint64_t bit_length = static_cast<std::uint64_t>(data.size()) * 8U;
    data.push_back(0x80U);
    while (data.size() % 64U != 56U) data.push_back(0U);
    for (int shift = 56; shift >= 0; shift -= 8)
        data.push_back(static_cast<std::uint8_t>(bit_length >> static_cast<unsigned>(shift)));
    std::array<std::uint32_t, 8> hash{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                                      0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    for (std::size_t offset = 0; offset < data.size(); offset += 64U) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            const auto at = offset + index * 4U;
            words[index] = (std::uint32_t(data[at]) << 24U) |
                           (std::uint32_t(data[at + 1U]) << 16U) |
                           (std::uint32_t(data[at + 2U]) << 8U) | data[at + 3U];
        }
        for (std::size_t index = 16; index < 64; ++index) {
            const auto s0 = rotate_right(words[index - 15], 7) ^ rotate_right(words[index - 15], 18) ^ (words[index - 15] >> 3U);
            const auto s1 = rotate_right(words[index - 2], 17) ^ rotate_right(words[index - 2], 19) ^ (words[index - 2] >> 10U);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        auto [a,b,c,d,e,f,g,h] = hash;
        for (std::size_t index = 0; index < 64; ++index) {
            const auto s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
            const auto choose = (e & f) ^ (~e & g);
            const auto temp1 = h + s1 + choose + constants[index] + words[index];
            const auto s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = s0 + majority;
            h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
        }
        hash[0]+=a; hash[1]+=b; hash[2]+=c; hash[3]+=d;
        hash[4]+=e; hash[5]+=f; hash[6]+=g; hash[7]+=h;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto value : hash) output << std::setw(8) << value;
    return output.str();
}

void populate(efis::Sample<double>& sample, efis::SignalId signal,
              efis::Unit unit, efis::MonoTime now, std::uint16_t channel = 0) {
    sample.value = static_cast<double>(static_cast<std::uint16_t>(signal));
    sample.source = efis::SourceId::simulation;
    sample.sampled_at = now;
    sample.received_at = now;
    sample.quality = efis::Quality::nominal;
    sample.validity = efis::Validity::simulated;
    sample.origin = {signal, 47, channel, unit, 1, true};
    sample.raw = sample.value;
}

void populate(efis::Sample<bool>& sample, efis::SignalId signal, efis::MonoTime now) {
    sample.value = true;
    sample.source = efis::SourceId::simulation;
    sample.sampled_at = now;
    sample.received_at = now;
    sample.quality = efis::Quality::nominal;
    sample.validity = efis::Validity::simulated;
    sample.origin = {signal, 47, 0, efis::Unit::boolean, 1, true};
    sample.raw = 1.0;
}

efis::FlightSnapshot canonical_flight(efis::MonoTime now) {
    efis::FlightSnapshot snapshot{};
    snapshot.sequence = 1;
    const std::array entries{
        std::tuple{&snapshot.attitude.roll_deg, efis::SignalId::roll, efis::Unit::degree},
        std::tuple{&snapshot.attitude.pitch_deg, efis::SignalId::pitch, efis::Unit::degree},
        std::tuple{&snapshot.attitude.heading_deg, efis::SignalId::heading, efis::Unit::degree},
        std::tuple{&snapshot.attitude.turn_rate_deg_s, efis::SignalId::turn_rate, efis::Unit::degree_per_second},
        std::tuple{&snapshot.attitude.lateral_acceleration_g, efis::SignalId::lateral_acceleration, efis::Unit::gravity},
        std::tuple{&snapshot.air_data.indicated_airspeed_kt, efis::SignalId::indicated_airspeed, efis::Unit::knot},
        std::tuple{&snapshot.air_data.pressure_altitude_ft, efis::SignalId::pressure_altitude, efis::Unit::foot},
        std::tuple{&snapshot.air_data.vertical_speed_fpm, efis::SignalId::vertical_speed, efis::Unit::foot_per_minute},
        std::tuple{&snapshot.air_data.baro_setting_hpa, efis::SignalId::baro_setting, efis::Unit::hectopascal},
        std::tuple{&snapshot.air_data.indicated_altitude_ft, efis::SignalId::indicated_altitude, efis::Unit::foot},
        std::tuple{&snapshot.position.latitude_deg, efis::SignalId::latitude, efis::Unit::degree},
        std::tuple{&snapshot.position.longitude_deg, efis::SignalId::longitude, efis::Unit::degree},
        std::tuple{&snapshot.position.ground_speed_kt, efis::SignalId::ground_speed, efis::Unit::knot},
        std::tuple{&snapshot.position.gnss_vertical_speed_fpm, efis::SignalId::gnss_vertical_speed, efis::Unit::foot_per_minute},
        std::tuple{&snapshot.guidance.commanded_roll_deg, efis::SignalId::commanded_roll, efis::Unit::degree},
        std::tuple{&snapshot.guidance.commanded_pitch_deg, efis::SignalId::commanded_pitch, efis::Unit::degree},
        std::tuple{&snapshot.guidance.selected_altitude_ft, efis::SignalId::selected_altitude, efis::Unit::foot},
        std::tuple{&snapshot.guidance.selected_course_deg, efis::SignalId::selected_course, efis::Unit::degree},
        std::tuple{&snapshot.guidance.selected_airspeed_kt, efis::SignalId::selected_airspeed, efis::Unit::knot},
        std::tuple{&snapshot.navigation.cdi_deviation_dots, efis::SignalId::cdi_deviation, efis::Unit::dot},
        std::tuple{&snapshot.navigation.glide_slope_dots, efis::SignalId::glide_slope_deviation, efis::Unit::dot}};
    for (const auto& [sample, signal, unit] : entries) populate(*sample, signal, unit, now);
    populate(snapshot.guidance.flight_director_active, efis::SignalId::flight_director_active, now);
    populate(snapshot.navigation.to_flag, efis::SignalId::to_flag, now);
    snapshot.health.level = efis::HealthLevel::normal;
    return snapshot;
}

efis::engine::EngineSnapshot canonical_engine(efis::MonoTime now) {
    efis::engine::EngineSnapshot snapshot{};
    const std::array entries{
        std::tuple{&snapshot.rpm, efis::SignalId::engine_rpm, efis::Unit::revolutions_per_minute},
        std::tuple{&snapshot.manifold_pressure_inhg, efis::SignalId::manifold_pressure, efis::Unit::inch_mercury},
        std::tuple{&snapshot.oil_pressure_psi, efis::SignalId::oil_pressure, efis::Unit::pound_per_square_inch},
        std::tuple{&snapshot.oil_temperature_c, efis::SignalId::oil_temperature, efis::Unit::celsius},
        std::tuple{&snapshot.fuel_pressure_psi, efis::SignalId::fuel_pressure, efis::Unit::pound_per_square_inch},
        std::tuple{&snapshot.fuel_flow_gph, efis::SignalId::fuel_flow, efis::Unit::gallon_per_hour},
        std::tuple{&snapshot.fuel_left_gal, efis::SignalId::fuel_quantity_left, efis::Unit::gallon},
        std::tuple{&snapshot.fuel_right_gal, efis::SignalId::fuel_quantity_right, efis::Unit::gallon},
        std::tuple{&snapshot.bus_voltage_v, efis::SignalId::bus_voltage, efis::Unit::volt},
        std::tuple{&snapshot.alternator_current_a, efis::SignalId::alternator_current, efis::Unit::ampere},
        std::tuple{&snapshot.cht_c[0], efis::SignalId::cylinder_head_temperature_1, efis::Unit::celsius},
        std::tuple{&snapshot.cht_c[1], efis::SignalId::cylinder_head_temperature_2, efis::Unit::celsius},
        std::tuple{&snapshot.cht_c[2], efis::SignalId::cylinder_head_temperature_3, efis::Unit::celsius},
        std::tuple{&snapshot.cht_c[3], efis::SignalId::cylinder_head_temperature_4, efis::Unit::celsius},
        std::tuple{&snapshot.egt_c[0], efis::SignalId::exhaust_gas_temperature_1, efis::Unit::celsius},
        std::tuple{&snapshot.egt_c[1], efis::SignalId::exhaust_gas_temperature_2, efis::Unit::celsius},
        std::tuple{&snapshot.egt_c[2], efis::SignalId::exhaust_gas_temperature_3, efis::Unit::celsius},
        std::tuple{&snapshot.egt_c[3], efis::SignalId::exhaust_gas_temperature_4, efis::Unit::celsius}};
    std::uint16_t channel = 1;
    for (const auto& [sample, signal, unit] : entries) populate(*sample, signal, unit, now, channel++);
    return snapshot;
}

std::uint32_t big_endian_u32(const std::vector<std::uint8_t>& wire, std::size_t offset) {
    return (std::uint32_t(wire[offset]) << 24U) | (std::uint32_t(wire[offset + 1]) << 16U) |
           (std::uint32_t(wire[offset + 2]) << 8U) | wire[offset + 3];
}
}

int main() {
    const auto vector_path = std::filesystem::path{EFIS_TEST_DATA_DIR} /
        "protocols/avionix-data-icd-v1-vectors.json";
    const auto frozen = read_text(vector_path);
    require(frozen.find("tools/protocol/validate_avionix_icd.py") != std::string::npos,
            "C++ test must consume the independently generated frozen vectors");
    const auto now = efis::MonoTime{} + 1s;
    constexpr std::uint64_t instance = 0x0102030405060708ULL;

    const auto ddp = efis::protocol::ddp::encode_snapshot(
        canonical_flight(now), efis::protocol::ddp::Profile::professional, 47, instance, now);
    require(ddp.size() == vector_size(frozen, "DDP_V2"), "DDP V2 frozen wire size differs");
    require(sha256(ddp) == vector_field(frozen, "DDP_V2", "wire_sha256"),
            "DDP V2 frozen wire digest differs");
    std::ostringstream ddp_crc;
    ddp_crc << std::hex << std::setfill('0') << std::setw(8) << big_endian_u32(ddp, 40);
    require(ddp_crc.str() == vector_field(frozen, "DDP_V2", "payload_crc32c"),
            "DDP V2 frozen payload CRC differs");

    const auto edp = efis::protocol::edp::encode_snapshot(
        canonical_engine(now), 47, instance, 1, now);
    require(edp.size() == vector_size(frozen, "EDP_V1"), "EDP V1 frozen wire size differs");
    require(sha256(edp) == vector_field(frozen, "EDP_V1", "wire_sha256"),
            "EDP V1 frozen wire digest differs");
    std::ostringstream edp_crc;
    edp_crc << std::hex << std::setfill('0') << std::setw(8) << big_endian_u32(edp, 38);
    require(edp_crc.str() == vector_field(frozen, "EDP_V1", "payload_crc32c"),
            "EDP V1 frozen payload CRC differs");

    std::cout << "AVIONIX ICD C++ frozen vector tests passed\n";
    return EXIT_SUCCESS;
}
