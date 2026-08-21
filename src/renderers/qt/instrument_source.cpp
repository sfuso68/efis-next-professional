#include "instrument_source.hpp"

#include "efis/core/signal_access.hpp"
#include "efis/instruments/dial.hpp"

#include <QVariant>

using namespace efis;

namespace {

int validity_of(const Sample<double>& sample) {
    return static_cast<int>(sample.validity);
}

}  // namespace

InstrumentSource::InstrumentSource(QObject* parent) : QObject(parent) {
    // La simulazione e' una sorgente registrata, non un provider chiamato
    // direttamente: cosi' lo stato che arriva agli strumenti e' gia' passato
    // dall'arbitrato, come quello del PFD.
    router_.set_simulation(simulation_);

    connect(&timer_, &QTimer::timeout, this, &InstrumentSource::step);
    // Venti volte al secondo: il simulatore non ha bisogno di piu', e il costo
    // del ridisegno va misurato su una cadenza realistica, non su una scelta
    // comoda.
    timer_.start(50);
    step();
}

void InstrumentSource::step() {
    const auto now = MonoClock::now();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - started_);
    snapshot_ = model_.normalize(router_.poll(now, elapsed), now);

    if (!failed_signal_.isEmpty()) {
        // Il guasto viene applicato allo snapshot, non al provider: e' cosi' che
        // si presenterebbe davvero, come una grandezza che smette di arrivare
        // mentre le altre continuano.
        auto fail = [](Sample<double>& sample) {
            sample.validity = Validity::unavailable;
        };
        if (failed_signal_ == QLatin1String("attitude")) {
            fail(snapshot_.attitude.roll_deg);
            fail(snapshot_.attitude.pitch_deg);
        } else if (failed_signal_ == QLatin1String("heading")) {
            fail(snapshot_.attitude.heading_deg);
        } else if (failed_signal_ == QLatin1String("airspeed")) {
            fail(snapshot_.air_data.indicated_airspeed_kt);
        } else if (failed_signal_ == QLatin1String("altitude")) {
            fail(snapshot_.air_data.indicated_altitude_ft);
        } else if (failed_signal_ == QLatin1String("turn")) {
            fail(snapshot_.attitude.turn_rate_deg_s);
        }
    }

    emit updated();
}

void InstrumentSource::injectFailure(const QString& signal) {
    failed_signal_ = failed_signal_ == signal ? QString{} : signal;
}

double InstrumentSource::airspeed() const {
    return snapshot_.air_data.indicated_airspeed_kt.value;
}
int InstrumentSource::airspeedValidity() const {
    return validity_of(snapshot_.air_data.indicated_airspeed_kt);
}

QVariantList InstrumentSource::airspeedBands() const {
    // Le velocita' del profilo LSA attivo: VSO 39, VS1 45, VFE 85, VNO 120,
    // VNE 140. Verranno lette dal profilo quando la pagina sara' configurabile;
    // qui sono scritte per rendere la verifica sul display riproducibile.
    QVariantList bands;
    auto band = [&bands](double from, double to, int kind) {
        bands.append(QVariant{QVariantList{from, to, kind}});
    };
    band(39.0, 85.0, 0);    // bianca: ipersostentatori estesi
    band(45.0, 120.0, 1);   // verde: funzionamento normale
    band(120.0, 140.0, 2);  // gialla: da usare con cautela
    band(140.0, 160.0, 3);  // rossa: oltre la velocita' da non superare
    return bands;
}

double InstrumentSource::altitude() const {
    return snapshot_.air_data.indicated_altitude_ft.value;
}
int InstrumentSource::altitudeValidity() const {
    return validity_of(snapshot_.air_data.indicated_altitude_ft);
}

QString InstrumentSource::baroSetting() const {
    const Sample<double>& setting = snapshot_.air_data.baro_setting_hpa;
    if (!setting.usable()) return {};
    return QString::number(static_cast<int>(setting.value + 0.5));
}

double InstrumentSource::verticalSpeed() const {
    return snapshot_.air_data.vertical_speed_fpm.value;
}
int InstrumentSource::verticalSpeedValidity() const {
    return validity_of(snapshot_.air_data.vertical_speed_fpm);
}

double InstrumentSource::roll() const { return snapshot_.attitude.roll_deg.value; }
int InstrumentSource::rollValidity() const {
    return validity_of(snapshot_.attitude.roll_deg);
}
double InstrumentSource::pitch() const { return snapshot_.attitude.pitch_deg.value; }
int InstrumentSource::pitchValidity() const {
    return validity_of(snapshot_.attitude.pitch_deg);
}

double InstrumentSource::heading() const { return snapshot_.attitude.heading_deg.value; }
int InstrumentSource::headingValidity() const {
    return validity_of(snapshot_.attitude.heading_deg);
}
double InstrumentSource::selectedCourse() const {
    return snapshot_.guidance.selected_course_deg.value;
}
int InstrumentSource::selectedCourseValidity() const {
    return validity_of(snapshot_.guidance.selected_course_deg);
}

double InstrumentSource::turnRate() const {
    return snapshot_.attitude.turn_rate_deg_s.value;
}
int InstrumentSource::turnRateValidity() const {
    return validity_of(snapshot_.attitude.turn_rate_deg_s);
}
double InstrumentSource::slip() const {
    return snapshot_.attitude.lateral_acceleration_g.value;
}
int InstrumentSource::slipValidity() const {
    return validity_of(snapshot_.attitude.lateral_acceleration_g);
}

int InstrumentSource::limitsProvenance() const {
    // Il profilo attivo dichiara le proprie velocita INDICATIVE, ereditate dal
    // catalogo legacy e mai confrontate con un POH. Verra letto dal profilo
    // quando la pagina sara configurabile; scriverlo qui a mano sarebbe stato
    // comodo dichiararle verificate, ed e esattamente cio che non si fa.
    return static_cast<int>(efis::instruments::LimitsProvenance::indicative);
}

bool InstrumentSource::simulated() const {
    return snapshot_.attitude.roll_deg.is_simulated();
}
