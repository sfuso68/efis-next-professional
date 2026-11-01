// Bounded bench replay only. No arbitrary endpoint, production port or source identity.
#include "efis/core/signal_access.hpp"
#include "efis/core/freshness_budget.hpp"
#include "efis/protocol/ddp_session.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRandomGenerator>
#include <QUdpSocket>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef Q_OS_UNIX
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#endif

using namespace efis;
using namespace std::chrono_literals;
namespace {
constexpr std::uint32_t node = 65000;
constexpr quint16 port = 49152;
void require(bool condition, const char* why) {
    if (!condition) throw std::runtime_error(why);
}
Sample<double> value(const QJsonObject& object, const char* key, const char* unit,
                     SignalId signal, Unit target, MonoTime now, std::uint64_t seq,
                     double factor = 1) {
    require(object[key].isObject(), "missing typed datum");
    const auto d = object[key].toObject();
    require(d["simulated"].isBool() && d["simulated"].toBool() && d["unit"].toString() == unit,
            "invalid SIM/unit");
    Sample<double> s;
    s.source = SourceId::simulation;
    s.sampled_at = s.received_at = now;
    s.sequence = seq;
    s.origin = {signal, node, static_cast<ChannelId>(signal), target, 0, true};
    if (d["validity"].toString() == "UNAVAILABLE") {
        require(d.contains("value") && d["value"].isNull(), "unavailable datum not null");
        return s;
    }
    require(d["validity"].toString() == "SIMULATED" && d["value"].isDouble(), "bad datum");
    s.raw = d["value"].toDouble();
    s.value = s.raw*factor;
    require(std::isfinite(s.value) && std::abs(s.value) < 1e7, "nonfinite/range");
    s.validity = Validity::simulated;
    s.quality = Quality::nominal;
    return s;
}
FlightSnapshot adapt(const QJsonObject& f, const QJsonObject& config, MonoTime now, std::uint64_t seq) {
    const auto truth = f["flight"].toObject(), world = f["world"].toObject();
    require(world["schema"].toString() == "fuso-avionics.sil-world.v1" &&
            world["simulated"].isBool() && world["simulated"].toBool() &&
            world["flight_approval"].isBool() && !world["flight_approval"].toBool(), "world not SIM");
    const auto sensors = world["sensors"].toObject(), nav = world["navigation"].toObject();
    FlightSnapshot s;
    s.timestamp = now; s.sequence = seq;
    s.attitude.roll_deg = value(truth, "roll_deg", "deg", SignalId::roll, Unit::degree, now, seq);
    s.attitude.pitch_deg = value(truth, "pitch_deg", "deg", SignalId::pitch, Unit::degree, now, seq);
    s.attitude.heading_deg = value(sensors, "heading_mag_deg", "degMAG", SignalId::heading, Unit::degree, now, seq);
    s.air_data.indicated_airspeed_kt = value(sensors, "ias_kt", "kt", SignalId::indicated_airspeed, Unit::knot, now, seq);
    s.air_data.pressure_altitude_ft = value(sensors, "pressure_altitude_ft", "ft", SignalId::pressure_altitude, Unit::foot, now, seq);
    s.air_data.indicated_altitude_ft = value(sensors, "indicated_altitude_ft", "ft", SignalId::indicated_altitude, Unit::foot, now, seq);
    // No geometric VS duplicated into baro/GNSS independent sensor channels.
    s.position.latitude_deg = value(nav["gnss"].toObject(), "lat", "deg", SignalId::latitude, Unit::degree, now, seq);
    s.position.longitude_deg = value(nav["gnss"].toObject(), "lon", "deg", SignalId::longitude, Unit::degree, now, seq);
    s.position.ground_speed_kt = value(nav["gnss"].toObject(), "gs_kt", "kt", SignalId::ground_speed, Unit::knot, now, seq);
    auto r = config["receiver"].toObject();
    QJsonObject setting{{"value", r["altimeter_hpa"]}, {"unit", "hPa"}, {"validity", "SIMULATED"}, {"simulated", true}};
    s.air_data.baro_setting_hpa = value(QJsonObject{{"baro", setting}}, "baro", "hPa", SignalId::baro_setting, Unit::hectopascal, now, seq);
    QJsonObject course{{"value", r["obs_deg"]}, {"unit", "degMAG"}, {"validity", "SIMULATED"}, {"simulated", true}};
    s.guidance.selected_course_deg = value(QJsonObject{{"obs", course}}, "obs", "degMAG", SignalId::selected_course, Unit::degree, now, seq);
    const auto selected = nav["selected_id"].toString();
    bool found = false;
    for (const auto radioValue : nav["radios"].toArray()) {
        const auto radio = radioValue.toObject();
        if (radio["id"].toString() != selected) continue;
        found = true;
        if (radio["kind"].toString() == "VOR") {
            // Explicit bench presentation convention: 10 degrees = five dots.
            s.navigation.cdi_deviation_dots = value(radio, "cdi", "full_scale", SignalId::cdi_deviation, Unit::dot, now, seq, 5);
            const auto flag = radio["to_from"].toObject();
            if (flag["validity"].toString() == "SIMULATED") {
                require(flag["value"].toString() == "TO" || flag["value"].toString() == "FROM", "bad TO/FROM");
                s.navigation.to_flag.value = flag["value"].toString() == "TO";
                s.navigation.to_flag.source = SourceId::simulation;
                s.navigation.to_flag.validity = Validity::simulated;
                s.navigation.to_flag.quality = Quality::nominal;
                s.navigation.to_flag.raw = s.navigation.to_flag.value ? 1 : 0;
                s.navigation.to_flag.sampled_at = s.navigation.to_flag.received_at = now;
                s.navigation.to_flag.sequence = seq;
                s.navigation.to_flag.origin = {SignalId::to_flag, node, 502, Unit::boolean, 0, true};
            }
        } else if (radio["kind"].toString() == "ILS") {
            s.navigation.cdi_deviation_dots = value(radio, "localizer", "full_scale", SignalId::cdi_deviation, Unit::dot, now, seq, 5);
            s.navigation.glide_slope_dots = value(radio, "glideslope", "full_scale", SignalId::glide_slope_deviation, Unit::dot, now, seq, 5);
        }
    }
    require(found, "selected radio missing");
    // Missing fields remain unavailable, never filled with plausible values.
    s.health.level = HealthLevel::warning;
    for (auto id : flight_signals()) {
        if (auto* datum = find_signal(s, id)) {
            if (!datum->usable()) ++s.health.unavailable_fields;
        }
    }
    return s;
}

#ifdef Q_OS_UNIX
QJsonObject read_live_line(MonoTime deadline) {
    QByteArray line;
    for (;;) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline-MonoClock::now()).count();
        require(remaining > 0, "live response timeout");
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        const auto ready = ::poll(&descriptor, 1, static_cast<int>(remaining));
        if (ready < 0 && errno == EINTR) continue;
        require(ready > 0, "live response timeout/poll");
        char bytes[4096];
        const auto received = ::read(STDIN_FILENO, bytes, sizeof(bytes));
        if (received < 0 && errno == EINTR) continue;
        require(received > 0, "live response EOF/read");
        line.append(bytes, static_cast<qsizetype>(received));
        require(line.size() <= 65536, "oversize live response");
        const auto newline = line.indexOf('\n');
        if (newline >= 0) {
            require(newline == line.size()-1, "unsolicited pipelined live response");
            break;
        }
    }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(line, &error);
    require(error.error == QJsonParseError::NoError && doc.isObject(), "bad live JSON");
    return doc.object();
}

void live_json(const QJsonObject& object) {
    std::cout << QJsonDocument(object).toJson(QJsonDocument::Compact).constData() << std::endl;
}

int live_stdio(const QByteArray& expected, bool send) {
    const auto hello = read_live_line(MonoClock::now()+10s);
    constexpr auto schema = "fuso-avionics.sil-live.v1";
    require(hello["schema"].toString() == schema && hello["kind"].toString() == "HELLO" &&
            hello["simulated"].isBool() && hello["simulated"].toBool() &&
            hello["flight_approval"].isBool() && !hello["flight_approval"].toBool(), "bad live hello");
    const auto canonical = hello["world_config_canonical"].toString().toUtf8();
    require(expected.size() == 64 && hello["world_config_sha256"].toString().toLatin1() == expected &&
            QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex() == expected, "live config digest");
    const auto config = QJsonDocument::fromJson(canonical).object();
    const auto sid = hello["session_id"].toString();
    const auto seconds = hello["seconds"].toDouble(-1);
    require(!sid.isEmpty() && sid.size() <= 64 && seconds >= 1 && seconds <= 120 && std::floor(seconds) == seconds,
            "live session/duration");
    auto instance = QRandomGenerator::system()->generate64();
    if (!instance) instance = 1;
    protocol::ddp::SnapshotPublisher publisher{{protocol::ddp::Profile::professional, node, instance, 1}};
    protocol::ddp::SnapshotConsumer test{{250ms, 1000ms, node}}, production{{250ms, 1000ms, 47}};
    QUdpSocket socket;
    const auto start = MonoClock::now();
    double maximum_rtt_ms = 0;
    const auto count = static_cast<std::uint64_t>(seconds)*20+1;
    for (std::uint64_t index = 0; index < count; ++index) {
        const auto nonce = QString::number(QRandomGenerator::system()->generate64(), 16).rightJustified(16, '0')+
                           QString::number(QRandomGenerator::system()->generate64(), 16).rightJustified(16, '0');
        const auto sent = MonoClock::now();
        live_json({{"schema", schema}, {"sequence", static_cast<qint64>(index)}, {"nonce", nonce}});
        const auto response = read_live_line(sent+FreshnessBudget::network_age_margin);
        const auto now = MonoClock::now();
        require(now-sent <= FreshnessBudget::network_age_margin, "late live response");
        maximum_rtt_ms = std::max(maximum_rtt_ms, std::chrono::duration<double, std::milli>(now-sent).count());
        require(response["schema"].toString() == schema && response["kind"].toString() == "FRAME" &&
                response["nonce"].toString() == nonce && response["frame"].isObject(), "live challenge mismatch");
        const auto frame = response["frame"].toObject();
        require(frame["schema"].toString() == "fuso-avionics.physics-sil.v1" &&
                frame["session_id"].toString() == sid && frame["sequence"].isDouble() &&
                frame["sequence"].toDouble(-1) == static_cast<double>(index) &&
                frame["sim_time_ms"].toDouble(-1) == static_cast<double>(index*50) &&
                frame["simulated"].isBool() && frame["simulated"].toBool() &&
                frame["flight_approval"].isBool() && !frame["flight_approval"].toBool() &&
                frame["backend"].toString() == "JSBSim-1.3.3" && frame["model"].toString() == "c172p" &&
                frame["profile_status"].toString() == "REFERENCE_UNVALIDATED", "live frame contract");
        // Reject acceleration/buffer bursts; the source must actually pace physical time.
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now-start).count();
        require(elapsed_ms+100 >= static_cast<std::int64_t>(index*50) &&
                elapsed_ms <= static_cast<std::int64_t>(index*50)+FreshnessBudget::transport_lost.count(), "live stream clock drift");
        // Conservatively preserve the ENTIRE measured request RTT as sensor age.
        const auto snapshot = adapt(frame, config, sent, index+1);
        const auto packet = publisher.publish(snapshot, now);
        require(static_cast<bool>(test.consume(packet, now)) && !production.consume(packet, now), "live consumer guard");
        if (send) {
            require(socket.writeDatagram(reinterpret_cast<const char*>(packet.data()), static_cast<qint64>(packet.size()),
                    QHostAddress::LocalHost, port) == static_cast<qint64>(packet.size()), "live loopback send");
        }
    }
    live_json({{"schema", schema}, {"kind", "COMPLETE"}, {"frames", static_cast<qint64>(count)},
               {"maximum_rtt_ms", maximum_rtt_ms}, {"simulated", true}, {"flight_approval", false},
               {"mode", send ? "LIVE_LOOPBACK" : "LIVE_CHECK_ONLY"}});
    return 0;
}
#endif
int run(const QString& root, const QByteArray& expected, bool send) {
    QFile mf(root+"/manifest.json"), file(root+"/frames.jsonl");
    require(mf.open(QIODevice::ReadOnly) && mf.size() < 65536 && file.open(QIODevice::ReadOnly) && file.size() <= 32*1024*1024,
            "missing/oversize replay");
    const auto manifest = QJsonDocument::fromJson(mf.readAll()).object();
    require(manifest["simulated"].isBool() && manifest["simulated"].toBool() &&
            manifest["flight_approval"].isBool() && !manifest["flight_approval"].toBool() &&
            manifest["status"].toString() == "COMPLETE_OFFLINE_REFERENCE", "bad manifest");
    require(expected.size() == 64 && manifest["frames_sha256"].toString().toLatin1() == expected, "expected SHA mismatch");
    const auto bytes = file.readAll();
    require(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() == expected, "replay digest mismatch");
    const auto config = manifest["world_config"].toObject();
    std::vector<QJsonObject> frames;
    for (const auto& line : bytes.split('\n')) {
        if (line.isEmpty()) continue;
        require(line.size() <= 65536, "oversize frame");
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(line, &error);
        require(error.error == QJsonParseError::NoError && doc.isObject(), "bad JSON");
        const auto f = doc.object();
        require(f["schema"].toString() == "fuso-avionics.physics-sil.v1" &&
                f["session_id"] == manifest["session_id"] && f["simulated"].isBool() && f["simulated"].toBool() &&
                f["flight_approval"].isBool() && !f["flight_approval"].toBool() &&
                f["sequence"].isDouble() && f["sequence"].toDouble() == static_cast<double>(frames.size()) &&
                f["sim_time_ms"].isDouble() && f["sim_time_ms"].toDouble() == static_cast<double>(frames.size()*50), "frame clock/identity/SIM");
        frames.push_back(f);
        require(frames.size() <= 2401, "bench replay max 120 seconds");
    }
    require(frames.size() >= 21 && static_cast<double>(frames.size()) == manifest["frames"].toDouble(), "frame count");
    auto instance = QRandomGenerator::system()->generate64();
    if (!instance) instance = 1;
    protocol::ddp::SnapshotPublisher publisher{{protocol::ddp::Profile::professional, node, instance, 1}};
    protocol::ddp::SnapshotConsumer test{{250ms, 1000ms, node}}, production{{250ms, 1000ms, 47}};
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto now = MonoClock::now();
        const auto s = adapt(frames[i], config, now, i+1);
        const auto d = publisher.publish(s, now);
        require(static_cast<bool>(test.consume(d, now)) && !production.consume(d, now), "consumer guard");
        require(test.latest()->air_data.indicated_airspeed_kt.is_simulated(), "SIM lost");
    }
    std::cout << "BENCH_REPLAY_PREFLIGHT=PASS frames=" << frames.size()
              << " source_node=65000 mode=" << (send ? "LOOPBACK_900S" : "CHECK_ONLY") << std::endl;
    if (!send) return 0;
    protocol::ddp::SnapshotPublisher live{{protocol::ddp::Profile::professional, node, instance, 1}};
    QUdpSocket socket;
    const auto start = MonoClock::now();
    for (std::uint64_t seq = 1; seq <= 18000; ++seq) {
        const auto deadline = start+(seq-1)*50ms;
        std::this_thread::sleep_until(deadline);
        const auto now = MonoClock::now();
        require(now-deadline <= 100ms, "scheduler late: stop instead of burst catchup");
        const auto snapshot = adapt(frames[(seq-1)%frames.size()], config, now, seq);
        const auto packet = live.publish(snapshot, now);
        require(socket.writeDatagram(reinterpret_cast<const char*>(packet.data()), static_cast<qint64>(packet.size()), QHostAddress::LocalHost, port)
                == static_cast<qint64>(packet.size()), "loopback send failed");
        if (seq % 200 == 0) std::cout << "BENCH_REPLAY_SENT=" << seq << " loop=" << (seq-1)/frames.size()
                                     << " SIM=1 flight_approval=false" << std::endl;
    }
    return 0;
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        if (argc == 3 && (QString(argv[2]) == "--live-check-stdio" || QString(argv[2]) == "--live-loopback-stdio")) {
#ifdef Q_OS_UNIX
            return live_stdio(QByteArray(argv[1]), QString(argv[2]) == "--live-loopback-stdio");
#else
            throw std::runtime_error("live stdio requires POSIX pipe deadline support");
#endif
        }
        require(argc == 4 && (QString(argv[3]) == "--check" || QString(argv[3]) == "--loopback-900s"),
                "usage: efis-sil-panel-replay VERIFIED_SESSION SHA256 --check|--loopback-900s");
        return run(QString::fromLocal8Bit(argv[1]), QByteArray(argv[2]), QString(argv[3]) == "--loopback-900s");
    } catch (const std::exception& e) {
        std::cerr << "BENCH_REPLAY_FAILED=" << e.what() << std::endl;
        return 2;
    }
}
