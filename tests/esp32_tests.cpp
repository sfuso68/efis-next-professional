// Prove dell'adapter del nodo sensori ESP32, primo passo di P4.
//
// I frame sono costruiti secondo docs/protocols/ESP32_CAN_V1.md, che a sua volta
// e' ricavato dal firmware reale. Non esiste ancora una registrazione dal bus,
// quindi queste prove verificano che l'adapter rispetti la specifica: il giorno
// in cui il nodo sara' collegato, una cattura vera resta da fare.

#include "efis/providers/esp32_can_adapter.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void put_i16(efis::can::Frame& frame, std::uint8_t offset, int value) {
    const auto raw = static_cast<std::uint16_t>(static_cast<std::int16_t>(value));
    frame.data[offset] = static_cast<std::uint8_t>(raw & 0xFF);
    frame.data[offset + 1] = static_cast<std::uint8_t>((raw >> 8) & 0xFF);
}

void put_i32(efis::can::Frame& frame, std::uint8_t offset, std::int32_t value) {
    const auto raw = static_cast<std::uint32_t>(value);
    for (std::uint8_t index = 0; index < 4; ++index) {
        frame.data[offset + index] = static_cast<std::uint8_t>((raw >> (8U * index)) & 0xFF);
    }
}

// 0x100 ATTITUDE. Beccheggio prima di rollio, come fa il firmware.
efis::can::Frame attitude(double pitch, double roll, double heading,
                          bool imu, bool baro, bool pitot,
                          std::uint8_t cal_sys, std::uint8_t cal_mag) {
    efis::can::Frame frame{};
    frame.id = efis::esp32_can::id_attitude;
    frame.length = 8;
    put_i16(frame, 0, static_cast<int>(pitch * 100));
    put_i16(frame, 2, static_cast<int>(roll * 100));
    put_i16(frame, 4, static_cast<int>(heading * 100));
    frame.data[6] = static_cast<std::uint8_t>((imu ? 1 : 0) | (baro ? 2 : 0) |
                                              (pitot ? 4 : 0));
    frame.data[7] = static_cast<std::uint8_t>(cal_sys | (cal_mag << 2) |
                                              (3 << 4) | (3 << 6));
    return frame;
}

// 0x101 AIR_DATA. La pressione viaggia con l'offset di 1000 hPa.
efis::can::Frame air_data(double airspeed_kt, int altitude_ft,
                          double pressure_hpa, double differential_pa) {
    efis::can::Frame frame{};
    frame.id = efis::esp32_can::id_air_data;
    frame.length = 8;
    put_i16(frame, 0, static_cast<int>(airspeed_kt * 10));
    put_i16(frame, 2, altitude_ft);
    put_i16(frame, 4, static_cast<int>((pressure_hpa - 1000.0) * 10));
    put_i16(frame, 6, static_cast<int>(differential_pa * 10));
    return frame;
}

efis::can::Frame health(std::uint16_t heartbeat, double load_g, double temperature_c) {
    efis::can::Frame frame{};
    frame.id = efis::esp32_can::id_health;
    frame.length = 8;
    frame.data[0] = static_cast<std::uint8_t>(heartbeat & 0xFF);
    frame.data[1] = static_cast<std::uint8_t>((heartbeat >> 8) & 0xFF);
    put_i16(frame, 2, static_cast<int>(load_g * 100));
    put_i16(frame, 4, static_cast<int>(temperature_c * 10));
    put_i16(frame, 6, 0);
    return frame;
}

efis::can::Frame gps_status(bool fix, std::uint8_t satellites, double hdop) {
    efis::can::Frame frame{};
    frame.id = efis::esp32_can::id_gps_status;
    frame.length = 8;
    frame.data[0] = fix ? 1 : 0;
    frame.data[1] = satellites;
    put_i16(frame, 2, static_cast<int>(hdop * 10));
    return frame;
}

efis::can::Frame gps_position(double latitude, double longitude) {
    efis::can::Frame frame{};
    frame.id = efis::esp32_can::id_gps_position;
    frame.length = 8;
    put_i32(frame, 0, static_cast<std::int32_t>(latitude * 1e7));
    put_i32(frame, 4, static_cast<std::int32_t>(longitude * 1e7));
    return frame;
}

bool close_to(double got, double expected, double tolerance = 0.02) {
    return std::abs(got - expected) < tolerance;
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    const auto epoch = efis::MonoTime{};

    // --- Scale e ordine dei byte --------------------------------------------
    {
        efis::Esp32CanAdapter adapter;
        adapter.consume(attitude(-4.25, 11.50, 236.75, true, true, true, 3, 3),
                        epoch + 1s);

        const auto& attitude_state = adapter.snapshot().attitude;
        require(close_to(attitude_state.pitch_deg.value, -4.25),
                "pitch must decode with its hundredth of a degree scale, sign included");
        require(close_to(attitude_state.roll_deg.value, 11.50),
                "roll must decode, and must not be swapped with pitch");
        require(close_to(attitude_state.heading_deg.value, 236.75),
                "heading must decode");
        require(attitude_state.roll_deg.origin.unit == efis::Unit::degree,
                "the unit must be declared, not implied by the field name");
        require(attitude_state.roll_deg.source == efis::SourceId::esp32_legacy,
                "samples must name the node they come from");
        require(attitude_state.roll_deg.origin.node == 21,
                "the node identifier must reach the sample");
        require(close_to(attitude_state.pitch_deg.raw, -425.0, 0.5),
                "the raw value must be kept as transmitted");
    }

    // --- L'offset di pressione va rimosso ------------------------------------
    //
    // Dimenticarlo produce 13 hPa, che si riconosce, oppure 1013 quando il
    // grezzo è vicino a zero, che non si riconosce affatto.
    {
        efis::Esp32CanAdapter adapter;
        adapter.consume(attitude(0, 0, 0, true, true, true, 3, 3), epoch + 1s);
        adapter.consume(air_data(87.4, 3200, 1013.2, 47.5), epoch + 1s);

        require(close_to(adapter.snapshot().air_data.indicated_airspeed_kt.value, 87.4),
                "indicated airspeed must decode with its tenth of a knot scale");
        require(close_to(adapter.snapshot().air_data.pressure_altitude_ft.value, 3200.0),
                "altitude must decode in feet");
        require(close_to(adapter.sensor_state().static_pressure_hpa.value, 1013.2, 0.15),
                "the 1000 hPa transmission offset must be removed on reading");
        require(close_to(adapter.sensor_state().differential_pressure_pa.value, 47.5),
                "the differential pressure is what the sensor actually measures");
    }

    // --- Il nodo dichiara il proprio stato e va creduto ----------------------
    //
    // Una fusione non calibrata produce numeri plausibili e privi di
    // significato: è precisamente ciò che non deve avere l'aspetto di una misura.
    {
        efis::Esp32CanAdapter adapter;
        adapter.consume(attitude(5, 5, 90, true, true, true, 0, 3), epoch + 1s);
        require(!adapter.snapshot().attitude.roll_deg.usable(),
                "an uncalibrated fusion must not produce a usable attitude");

        adapter.consume(attitude(5, 5, 90, true, true, true, 2, 3), epoch + 2s);
        require(adapter.snapshot().attitude.roll_deg.validity == efis::Validity::degraded,
                "a partially calibrated fusion must be degraded, not simply valid");

        adapter.consume(attitude(5, 5, 90, true, true, true, 3, 3), epoch + 3s);
        require(adapter.snapshot().attitude.roll_deg.validity == efis::Validity::valid,
                "a fully calibrated fusion must be valid");
    }

    // --- Prua e assetto hanno calibrazioni diverse ---------------------------
    //
    // Sul BNO055 la prua dipende dal magnetometro, rollio e beccheggio no.
    // Trattarli insieme significherebbe o scartare un assetto buono o
    // presentare una prua che non lo è.
    {
        efis::Esp32CanAdapter adapter;
        adapter.consume(attitude(5, 5, 90, true, true, true, 3, 0), epoch + 1s);
        require(adapter.snapshot().attitude.roll_deg.usable(),
                "a good attitude must survive an uncalibrated magnetometer");
        require(!adapter.snapshot().attitude.heading_deg.usable(),
                "an uncalibrated magnetometer must not produce a usable heading");
    }

    // --- I bit di sensore governano le grandezze che producono ---------------
    {
        efis::Esp32CanAdapter adapter;
        adapter.consume(attitude(0, 0, 0, true, false, false, 3, 3), epoch + 1s);
        adapter.consume(air_data(87.4, 3200, 1013.2, 47.5), epoch + 1s);
        require(!adapter.snapshot().air_data.pressure_altitude_ft.usable(),
                "a barometer declared faulty must not produce a usable altitude");
        require(!adapter.snapshot().air_data.indicated_airspeed_kt.usable(),
                "a pitot declared faulty must not produce a usable airspeed");
        require(adapter.snapshot().attitude.roll_deg.usable(),
                "a faulty barometer must not invalidate the attitude");
    }

    // --- Senza fix la posizione non esiste ----------------------------------
    {
        efis::Esp32CanAdapter adapter;
        adapter.consume(gps_status(false, 0, 99.9), epoch + 1s);
        adapter.consume(gps_position(45.6484, 12.1944), epoch + 1s);
        require(!adapter.snapshot().position.latitude_deg.usable(),
                "coordinates without a fix are not a position");

        adapter.consume(gps_status(true, 11, 1.2), epoch + 2s);
        adapter.consume(gps_position(45.6484, 12.1944), epoch + 2s);
        require(adapter.snapshot().position.latitude_deg.usable(),
                "with a fix the position becomes usable");
        require(close_to(adapter.snapshot().position.latitude_deg.value, 45.6484, 1e-5),
                "latitude must decode from its 1e-7 degree scale");
        require(adapter.sensor_state().gnss_satellites == 11,
                "the satellite count must be kept");
    }

    // --- Il contatore a 16 bit avvolge, e non è un guasto -------------------
    //
    // Interpretare l'avvolgimento come una regressione dichiarerebbe un guasto
    // ogni 6553 secondi su un nodo perfettamente sano.
    {
        efis::Esp32CanAdapter adapter;
        adapter.consume(health(65534, 1.0, 21.0), epoch + 1s);
        adapter.consume(health(65535, 1.0, 21.0), epoch + 2s);
        adapter.consume(health(0, 1.0, 21.0), epoch + 3s);
        adapter.consume(health(1, 1.0, 21.0), epoch + 4s);

        require(adapter.statistics().heartbeat_wraps == 1,
                "the wrap must be recognised, once");
        const auto status = adapter.health().status(21);
        require(status.has_value() && status->state == efis::NodeState::online,
                "a node whose counter wrapped must stay online");
        require(status->regressions == 0,
                "the wrap must not be counted as a sequence regression");
    }

    // --- Un frame troncato non vale zero ------------------------------------
    {
        efis::Esp32CanAdapter adapter;
        auto truncated = air_data(87.4, 3200, 1013.2, 47.5);
        truncated.length = 4;
        adapter.consume(truncated, epoch + 1s);
        require(adapter.statistics().frames_truncated == 1,
                "a short frame must be counted, not completed with zeros");
        require(adapter.snapshot().air_data.indicated_airspeed_kt.origin.signal ==
                    efis::SignalId::unknown,
                "a short frame must not produce samples at all");
    }

    // --- I comandi verso il nodo vengono ignorati ---------------------------
    //
    // L'adapter è in sola lettura: il frame di comando esiste nel protocollo,
    // e questa classe non lo costruisce né lo interpreta.
    {
        efis::Esp32CanAdapter adapter;
        efis::can::Frame command{};
        command.id = efis::esp32_can::id_command;
        command.length = 8;
        adapter.consume(command, epoch + 1s);
        require(adapter.statistics().frames_ignored == 1,
                "a command directed at the node must be ignored and counted");
        require(adapter.statistics().frames_applied == 0,
                "a command must not be applied as if it were a measurement");
    }

    // --- Il silenzio invalida quello che il nodo aveva lasciato -------------
    {
        efis::Esp32CanAdapter adapter;
        adapter.consume(attitude(5, 5, 90, true, true, true, 3, 3), epoch + 1s);
        adapter.consume(health(1, 1.0, 21.0), epoch + 1s);
        require(adapter.snapshot().attitude.roll_deg.usable(),
                "fresh data must be usable");

        adapter.update(epoch + 30s);
        require(!adapter.snapshot().attitude.roll_deg.usable(),
                "the last good attitude of a silent node must stop being usable");

        bool lost = false;
        for (const auto& event : adapter.node_events()) {
            if (event.current == efis::NodeState::lost) lost = true;
        }
        require(lost, "prolonged silence must be recorded as the loss of the node");
    }

    // --- La diagnostica del nodo viene conservata ---------------------------
    {
        efis::Esp32CanAdapter adapter;
        efis::can::Frame diagnostics{};
        diagnostics.id = efis::esp32_can::id_diagnostics;
        diagnostics.length = 8;
        diagnostics.data[0] = 4;    // firmware major
        diagnostics.data[1] = 0;    // firmware minor
        diagnostics.data[2] = 3;    // reset da watchdog
        diagnostics.data[3] = 12;   // avvii
        diagnostics.data[4] = 2;    // crash
        diagnostics.data[5] = 0x40; // heap, byte basso
        diagnostics.data[6] = 0x01; // heap, byte alto: 320 kB
        adapter.consume(diagnostics, epoch + 1s);

        const auto& state = adapter.sensor_state();
        require(state.diagnostics_received, "the diagnostic frame must be recognised");
        require(state.firmware_major == 4 && state.firmware_minor == 0,
                "the firmware version must be kept: the protocol carries no version field");
        require(state.reset_reason == 3 && state.crash_count == 2,
                "a node restarting under watchdog must be visible, not silent");
        require(state.free_heap_kb == 320, "the free heap must decode as a 16 bit value");
    }

    std::cout << "All EFIS ESP32 adapter tests passed\n";
}
