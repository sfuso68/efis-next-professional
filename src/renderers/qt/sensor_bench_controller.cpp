#include "sensor_bench_controller.hpp"

#include "efis/display/display_model.hpp"
#include "efis/io/byte_source.hpp"

#include <QFileInfo>

#include <exception>

namespace {

// Aggiornamento a cinque hertz. Il banco si guarda, non si pilota: piu' veloce
// renderebbe i numeri illeggibili e ruberebbe tempo al PFD, che sullo stesso
// display ha la precedenza.
constexpr int refresh_interval_ms = 200;

QString source_label(efis::SourceId source) {
    switch (source) {
        case efis::SourceId::unknown: return QStringLiteral("—");
        case efis::SourceId::simulation: return QStringLiteral("SIM");
        case efis::SourceId::pixhawk_primary: return QStringLiteral("PIXHAWK");
        case efis::SourceId::esp32_legacy: return QStringLiteral("ESP32");
        case efis::SourceId::dlvr_legacy: return QStringLiteral("DLVR");
        case efis::SourceId::nav_rf: return QStringLiteral("NAV RF");
        case efis::SourceId::nav_synthetic: return QStringLiteral("NAV SYN");
    }
    return QStringLiteral("—");
}

}  // namespace

SensorBenchController::SensorBenchController(QObject* parent) : QObject(parent) {
    timer_.setInterval(refresh_interval_ms);
    connect(&timer_, &QTimer::timeout, this, &SensorBenchController::pump);
}

SensorBenchController::~SensorBenchController() = default;

QString SensorBenchController::defaultSource() const {
    // Percorso configurabile, con un ripiego che punta alla registrazione di
    // riferimento distribuita con il progetto.
    if (const auto* configured = std::getenv("EFIS_BENCH_REPLAY")) {
        if (*configured != '\0') return QString::fromLocal8Bit(configured);
    }
    return QStringLiteral("data/mavlink/arduplane-4.7.0-usb-20260821.mav");
}

bool SensorBenchController::startReplay(const QString& path) {
    stop();

    const QFileInfo info{path};
    if (!info.exists() || !info.isFile()) {
        message_ = tr("Registrazione non trovata: %1").arg(path);
        emit changed();
        return false;
    }

    try {
        auto source = std::make_unique<efis::io::RecordedByteSource>(
            std::filesystem::path{path.toStdString()});
        bench_ = std::make_unique<efis::bench::SensorBench>(std::move(source));
    } catch (const std::exception& error) {
        // Un banco che non parte deve dirlo. Restare con le righe vuote
        // lascerebbe credere che il collegamento ci sia e taccia, che e' una
        // diagnosi diversa e sbagliata.
        message_ = tr("Sorgente non leggibile: %1").arg(QString::fromUtf8(error.what()));
        emit changed();
        return false;
    }

    source_name_ = info.fileName();
    message_ = tr("In riproduzione");
    exhausted_ = false;
    view_ = {};
    timer_.start();
    pump();
    return true;
}

void SensorBenchController::stop() {
    timer_.stop();
    bench_.reset();
    view_ = {};
    exhausted_ = false;
    source_name_.clear();
    message_.clear();
    emit changed();
}

void SensorBenchController::pump() {
    if (bench_ == nullptr) return;

    bench_->pump(efis::MonoClock::now());
    view_ = bench_->view();

    if (bench_->exhausted() && !exhausted_) {
        exhausted_ = true;
        // La riproduzione finisce, il banco no: si continua ad aggiornare, cosi'
        // i valori invecchiano e i nodi vengono dichiarati perduti sotto gli
        // occhi di chi guarda. E' il comportamento corretto, perche' e' quello
        // che succederebbe se il collegamento cadesse davvero.
        message_ = tr("Registrazione terminata, i valori ora invecchiano");
    }
    emit changed();
}

QString SensorBenchController::gnssSummary() const {
    if (view_.gnss_fix_type >= 3) {
        return tr("FIX 3D • %1 satelliti").arg(view_.gnss_satellites);
    }
    if (view_.gnss_fix_type <= 0) return tr("NESSUN FIX");
    return tr("FIX INSUFFICIENTE (%1) • %2 satelliti")
        .arg(view_.gnss_fix_type)
        .arg(view_.gnss_satellites);
}

QString SensorBenchController::airDataSummary() const {
    if (!view_.air_data_present) return tr("NON DICHIARATO");
    // Con ARSPD_USE a zero il controllo di volo puo' dichiarare non sano un
    // sensore perfettamente funzionante: dal solo bit le due cose non si
    // distinguono, e la UI non deve fingere di saperlo.
    return view_.air_data_healthy
               ? tr("PRESENTE • DICHIARATO SANO")
               : tr("PRESENTE • DICHIARATO NON SANO O NON IN USO");
}

QVariantList SensorBenchController::rows() const {
    QVariantList out;
    out.reserve(static_cast<qsizetype>(view_.rows.size()));
    for (const efis::bench::BenchRow& row : view_.rows) {
        QVariantMap entry;
        entry[QStringLiteral("name")] = QString::fromLatin1(row.name());
        entry[QStringLiteral("value")] = row.value;
        entry[QStringLiteral("raw")] = row.raw;
        entry[QStringLiteral("unit")] = QString::fromLatin1(row.symbol());
        entry[QStringLiteral("validity")] =
            QString::fromLatin1(efis::display::validity_label(row.validity));
        entry[QStringLiteral("source")] = source_label(row.source);
        entry[QStringLiteral("node")] = static_cast<int>(row.node);
        entry[QStringLiteral("ageMs")] = static_cast<qlonglong>(row.age.count());
        entry[QStringLiteral("usable")] = row.usable;
        entry[QStringLiteral("simulated")] = row.simulated;
        entry[QStringLiteral("note")] = QString::fromUtf8(row.note_text());
        out.append(entry);
    }
    return out;
}

QVariantList SensorBenchController::nodes() const {
    QVariantList out;
    out.reserve(static_cast<qsizetype>(view_.nodes.size()));
    for (const efis::NodeStatus& node : view_.nodes) {
        QVariantMap entry;
        entry[QStringLiteral("node")] = static_cast<int>(node.node);
        entry[QStringLiteral("state")] =
            QString::fromLatin1(efis::node_state_name(node.state));
        entry[QStringLiteral("online")] = efis::is_usable(node.state);
        entry[QStringLiteral("accepted")] = static_cast<qlonglong>(node.accepted);
        entry[QStringLiteral("missed")] = static_cast<qlonglong>(node.missed);
        entry[QStringLiteral("duplicates")] = static_cast<qlonglong>(node.duplicates);
        out.append(entry);
    }
    return out;
}

QVariantMap SensorBenchController::link() const {
    QVariantMap out;
    out[QStringLiteral("frames")] = static_cast<qlonglong>(view_.frames);
    out[QStringLiteral("crcErrors")] = static_cast<qlonglong>(view_.crc_errors);
    out[QStringLiteral("unknownMessages")] =
        static_cast<qlonglong>(view_.unknown_messages);
    out[QStringLiteral("droppedBytes")] = static_cast<qlonglong>(view_.dropped_bytes);
    return out;
}
