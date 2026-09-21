#include "pfd_controller.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void select_profile(const QString& path) {
    qputenv("EFIS_AIRCRAFT_PROFILE", path.toUtf8());
    qputenv("EFIS_SIMULATION_ROLE", "DISABLED");
    qputenv("EFIS_RECORDER_STATUS", "");
}

void write_file(const QString& path, const QByteArray& content) {
    QFile file{path};
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "temporary aircraft profile must be writable");
    require(file.write(content) == content.size(),
            "temporary aircraft profile must be complete");
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication application{argc, argv};
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory must exist");

    select_profile(directory.filePath("absent.json"));
    PfdController absent;
    require(absent.aircraftSpeedProfile().isEmpty(),
            "an absent profile must expose no numeric V-speeds");
    require(!absent.aircraftSpeedProfileAvailable() &&
                absent.aircraftSpeedProfileMessage() == QStringLiteral("V-SPEEDS UNAVAILABLE"),
            "an absent profile must expose a visible unavailable state");

    const auto corrupt_path = directory.filePath("corrupt.json");
    write_file(corrupt_path, "{not-json");
    select_profile(corrupt_path);
    PfdController corrupt;
    require(corrupt.aircraftSpeedProfile().isEmpty(),
            "a corrupt profile must expose no numeric V-speeds");
    require(!corrupt.aircraftSpeedProfileAvailable(),
            "a corrupt profile must remain explicitly unavailable");

    const auto incomplete_path = directory.filePath("incomplete.json");
    write_file(incomplete_path, R"json({
      "aircraft":{"manufacturer":"WRONG","model":"MIXED"},
      "engine":{"manufacturer":"SHOULD","model":"NOT APPLY"},
      "systems":{"landing_gear":{"installed":true,"retractable":true}},
      "speeds_kt":{"VNE":140}
    })json");
    select_profile(incomplete_path);
    PfdController incomplete;
    require(incomplete.aircraftSpeedProfile().isEmpty(),
            "an incomplete profile must not mix one V-speed with defaults");
    require(incomplete.engineProfileName() == QStringLiteral("UNCONFIGURED") &&
                !incomplete.landingGearInstalled(),
            "an incomplete profile must not partially mutate the controller");

    const auto complete_path = directory.filePath("complete.json");
    write_file(complete_path, R"json({
      "catalog_version":"test-v1",
      "state":"VERIFIED",
      "aircraft":{"manufacturer":"TEST","model":"AIRCRAFT"},
      "engine":{"manufacturer":"TEST","model":"ENGINE"},
      "systems":{"landing_gear":{"installed":true,"retractable":false}},
      "speeds_kt":{"VSO":39,"VS1":45,"VFE":85,"VX":62,"VY":68,
        "VA":105,"VNO":120,"VNE":140,"BEST_GLIDE":65,"VREF":55}
    })json");
    select_profile(complete_path);
    PfdController complete;
    require(complete.aircraftSpeedProfileAvailable() &&
                complete.aircraftSpeedProfile().size() == 10 &&
                complete.aircraftSpeedProfile().value("vne").toDouble() == 140.0,
            "a complete coherent profile must be applied as one unit");

    std::cout << "PFD aircraft profile fail-closed tests passed\n";
}
