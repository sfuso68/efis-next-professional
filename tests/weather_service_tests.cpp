#include "weather_service.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QStandardPaths>
#include <QTimer>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("efis-weather-service-tests"));
    QStandardPaths::setTestModeEnabled(true);
    qputenv("EFIS_WEATHER_SIMULATION_IO_DELAY_MS", "400");
    const auto outputRoot = QDir::currentPath() + QStringLiteral("/weather-service-test-runtime");
    QDir(outputRoot).removeRecursively();
    qputenv("EFIS_WEATHER_SIMULATION_DIR", outputRoot.toUtf8());

    try {
        require(WeatherService::classifyRadarAgeSeconds(-6) == QStringLiteral("CLOCK SKEW"),
                "future radar timestamp outside tolerance must fail closed");
        require(WeatherService::classifyRadarAgeSeconds(-5) == QStringLiteral("CURRENT"),
                "small clock jitter must remain inside tolerance");
        require(WeatherService::classifyRadarAgeSeconds(600) == QStringLiteral("CURRENT"),
                "600 seconds must remain CURRENT");
        require(WeatherService::classifyRadarAgeSeconds(601) == QStringLiteral("STALE"),
                "601 seconds must become STALE");
        require(WeatherService::classifyRadarAgeSeconds(1200) == QStringLiteral("STALE"),
                "1200 seconds must remain STALE");
        require(WeatherService::classifyRadarAgeSeconds(1201) == QStringLiteral("EXPIRED"),
                "1201 seconds must become EXPIRED");
        {
            WeatherService weather;
            QElapsedTimer callback;
            callback.start();
            weather.setSimulation(true);
            require(callback.elapsed() < 150,
                    "simulation callback blocked on the injected worker I/O delay");

            QEventLoop loop;
            QTimer deadline;
            deadline.setSingleShot(true);
            QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(&weather, &WeatherService::changed, &loop, [&] {
                if (!weather.radarSource().isEmpty())
                    loop.quit();
            });
            deadline.start(3000);
            loop.exec();

            if (weather.radarSource().isEmpty()) {
                std::cerr << "weather state=" << weather.radarState().toStdString()
                          << " message=" << weather.radarMessage().toStdString()
                          << " phase=" << weather.simulationPhase() << '\n';
                throw std::runtime_error("asynchronous simulation frame was not published");
            }
            require(weather.radarSource().contains(QStringLiteral("radar-sim-vmi.png")),
                    "unexpected simulation frame path");
            require(weather.radarState() == QStringLiteral("SIMULATED"),
                    "simulation frame did not publish SIMULATED state");
            require(weather.simulationPhase() >= 1, "simulation phase did not advance");
        }
        QDir(outputRoot).removeRecursively();
        std::cout << "weather_service_tests: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "weather_service_tests: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
