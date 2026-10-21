#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QTimeZone>
#include <QVariantList>
#include <cmath>

namespace weather_reports {
// Development display policy, not a regulatory or operational validity limit.
inline QString ageState(const QDateTime& observation, const QDateTime& now,
                        qint64 currentSeconds, qint64 staleSeconds) {
    if (!observation.isValid() || !now.isValid()) return QStringLiteral("UNKNOWN TIME");
    const auto age = observation.msecsTo(now);
    if (age < -5000) return QStringLiteral("CLOCK SKEW");
    if (age <= currentSeconds*1000) return QStringLiteral("CURRENT");
    if (age <= staleSeconds*1000) return QStringLiteral("STALE");
    return QStringLiteral("EXPIRED");
}
inline QDateTime utcTime(const QJsonValue& value) {
    if (value.isDouble()) {
        const auto seconds = value.toDouble();
        if (!std::isfinite(seconds) || seconds <= 0 || seconds >= 4102444800.0
            || std::floor(seconds) != seconds) return {};
        return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds), QTimeZone::utc());
    }
    if (!value.isString()) return {};
    auto text = value.toString();
    static const QRegularExpression zoned(QStringLiteral("(?:Z|[+-][0-9]{2}:[0-9]{2})$"));
    if (!zoned.match(text).hasMatch()) return {};
    if (text.size() > 10 && text.at(10) == QLatin1Char(' ')) text[10] = QLatin1Char('T');
    return QDateTime::fromString(text, Qt::ISODateWithMs).toUTC();
}
inline bool displayable(const QString& state) {
    return state == QStringLiteral("CURRENT") || state == QStringLiteral("STALE");
}

class Store final {
    struct Product {
        QString raw;
        QString category;
        QDateTime observation;
        QDateTime from;
        QDateTime to;
        QDateTime received;
    };
    struct Station { Product metar; Product taf; };
    QMap<QString, Station> rows_;
public:
    void clear() { rows_.clear(); }
    bool merge(bool taf, const QByteArray& payload, const QDateTime& received) {
        if (payload.size() > 512*1024) return false;
        const auto doc = QJsonDocument::fromJson(payload);
        if (!doc.isArray() || doc.array().size() > 16) return false;
        bool accepted = false;
        for (const auto& value : doc.array()) {
            if (!value.isObject()) continue;
            const auto item = value.toObject();
            const auto icao = item.value(QStringLiteral("icaoId")).toString();
            // Production currently requests these two stations only.
            if (icao != QStringLiteral("LIPH") && icao != QStringLiteral("LIPZ")) continue;
            Product next;
            next.raw = item.value(taf ? QStringLiteral("rawTAF") : QStringLiteral("rawOb")).toString();
            if (next.raw.isEmpty() || next.raw.size() > 8192) continue;
            next.observation = utcTime(item.value(taf ? QStringLiteral("issueTime") : QStringLiteral("obsTime")));
            // obsTime is the observation, reportTime is not a substitute for it.
            next.from = utcTime(item.value(QStringLiteral("validTimeFrom")));
            next.to = utcTime(item.value(QStringLiteral("validTimeTo")));
            next.category = item.value(QStringLiteral("fltCat")).toString();
            if (next.category != QStringLiteral("VFR") && next.category != QStringLiteral("MVFR")
                && next.category != QStringLiteral("IFR") && next.category != QStringLiteral("LIFR"))
                next.category.clear();
            next.received = received;
            auto& previous = taf ? rows_[icao].taf : rows_[icao].metar;
            if (previous.observation.isValid() && previous.observation <= received.addSecs(5)
                && (!next.observation.isValid() || next.observation < previous.observation)) continue;
            previous = next;
            accepted = true;
        }
        return accepted;
    }
    QVariantList view(const QDateTime& now) const {
        QVariantList result;
        for (auto it=rows_.cbegin(); it!=rows_.cend(); ++it) {
            const auto& metar = it->metar;
            const auto& taf = it->taf;
            auto ms = metar.raw.isEmpty() ? QStringLiteral("NO DATA") : ageState(metar.observation,now,3600,7200);
            auto ts = taf.raw.isEmpty() ? QStringLiteral("NO DATA") : ageState(taf.observation,now,21600,43200);
            if (!taf.raw.isEmpty() && ts != QStringLiteral("CLOCK SKEW")) {
                if (!taf.from.isValid() || !taf.to.isValid() || taf.from >= taf.to) ts=QStringLiteral("UNKNOWN TIME");
                else if (now >= taf.to) ts=QStringLiteral("EXPIRED");
                else if (now < taf.from) ts=QStringLiteral("NOT YET VALID");
            }
            const auto minutes = [&now](const QDateTime& time) {
                return now.isValid() && time.isValid() && time.msecsTo(now)>=-5000 ? static_cast<int>(qMax<qint64>(0,time.secsTo(now))/60) : -1;
            };
            result << QVariantMap{{QStringLiteral("icao"),it.key()},
                {QStringLiteral("metar"),displayable(ms)?metar.raw:QString()},
                {QStringLiteral("taf"),displayable(ts)?taf.raw:QString()},
                {QStringLiteral("rawMetar"),metar.raw},{QStringLiteral("rawTaf"),taf.raw},
                {QStringLiteral("metarState"),ms},{QStringLiteral("tafState"),ts},
                {QStringLiteral("metarAgeMinutes"),minutes(metar.observation)},
                {QStringLiteral("tafAgeMinutes"),minutes(taf.observation)},
                {QStringLiteral("category"),ms==QStringLiteral("CURRENT")?metar.category:QString()},
                {QStringLiteral("observed"),metar.observation.toString(Qt::ISODate)},
                {QStringLiteral("metarReceivedUtc"),metar.received.toString(Qt::ISODate)},
                {QStringLiteral("tafReceivedUtc"),taf.received.toString(Qt::ISODate)},
                {QStringLiteral("tafValidFrom"),taf.from.toString(Qt::ISODate)},
                {QStringLiteral("tafValidTo"),taf.to.toString(Qt::ISODate)},
                {QStringLiteral("simulated"),false},{QStringLiteral("safetyAuthority"),false}};
        }
        return result;
    }
};
} // namespace weather_reports
