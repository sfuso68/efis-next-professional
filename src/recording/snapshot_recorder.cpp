#include "efis/recording/snapshot_recorder.hpp"

#include <chrono>
#include <ostream>

namespace efis::recording {
namespace {

long long micros(MonoTime value) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               value.time_since_epoch()).count();
}

template <class T>
int id(T value) { return static_cast<int>(value); }

}  // namespace

void SnapshotRecorder::write_header(std::ostream& out) {
    out << "EFIS_RECORD_V1\n";
    out << "type,mono_us,sequence,ias_kt,ias_source,ias_validity,alt_ft,alt_source,alt_validity,heading_deg,heading_source,heading_validity,alerts\n";
}

void SnapshotRecorder::write_snapshot(std::ostream& out,
                                      const FlightSnapshot& s) {
    out << "snapshot," << micros(s.timestamp) << ',' << s.sequence << ','
        << s.air_data.indicated_airspeed_kt.value << ','
        << id(s.air_data.indicated_airspeed_kt.source) << ','
        << id(s.air_data.indicated_airspeed_kt.validity) << ','
        << s.air_data.pressure_altitude_ft.value << ','
        << id(s.air_data.pressure_altitude_ft.source) << ','
        << id(s.air_data.pressure_altitude_ft.validity) << ','
        << s.attitude.heading_deg.value << ','
        << id(s.attitude.heading_deg.source) << ','
        << id(s.attitude.heading_deg.validity) << ',';
    for (std::size_t index = 0; index < s.alerts.active.size(); ++index) {
        if (index != 0) out << ';';
        out << s.alerts.active[index].code;
    }
    out << '\n';
}

void SnapshotRecorder::write_source_change(std::ostream& out,
                                           const SourceChangeEvent& e) {
    out << "source_change," << micros(e.timestamp) << ",0,"
        << id(e.previous) << ',' << id(e.selected) << ',' << id(e.reason)
        << ',' << id(e.signal) << ",,,,,,\n";
}

}  // namespace efis::recording
