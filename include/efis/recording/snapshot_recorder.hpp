#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/core/source_arbitrator.hpp"

#include <iosfwd>

namespace efis::recording {

class SnapshotRecorder final {
public:
    static void write_header(std::ostream& output);
    static void write_snapshot(std::ostream& output, const FlightSnapshot& snapshot);
    static void write_source_change(std::ostream& output,
                                    const SourceChangeEvent& event);
};

}  // namespace efis::recording
