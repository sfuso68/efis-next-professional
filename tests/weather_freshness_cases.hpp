#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <stdexcept>

inline void weatherFreshnessCases() {
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    const auto now=QDateTime::fromString(QStringLiteral("2026-10-21T12:00:00Z"),Qt::ISODate);
    check(weather_reports::ageState(now.addSecs(-3600).addMSecs(-1),now,3600,7200)==QStringLiteral("STALE"),"subsecond expiry boundary must not truncate to CURRENT");
    check(weather_reports::ageState(now.addMSecs(5001),now,3600,7200)==QStringLiteral("CLOCK SKEW"),"subsecond clock tolerance must fail closed");
    const auto payload=[&](bool taf,qint64 age,qint64 from=-3600,qint64 to=3600){
        QJsonObject item{{QStringLiteral("icaoId"),QStringLiteral("LIPH")}};
        if(taf){item[QStringLiteral("rawTAF")]=QStringLiteral("TAF LIPH TEST");item[QStringLiteral("issueTime")]=now.addSecs(-age).toString(Qt::ISODate);item[QStringLiteral("validTimeFrom")]=now.addSecs(from).toSecsSinceEpoch();item[QStringLiteral("validTimeTo")]=now.addSecs(to).toSecsSinceEpoch();}
        else{item[QStringLiteral("rawOb")]=QStringLiteral("LIPH METAR TEST");item[QStringLiteral("obsTime")]=now.addSecs(-age).toSecsSinceEpoch();item[QStringLiteral("fltCat")]=QStringLiteral("VFR");}
        return QJsonDocument(QJsonArray{item}).toJson();
    };
    for(const auto& entry:QList<QPair<qint64,QString>>{{3600,"CURRENT"},{3601,"STALE"},{7200,"STALE"},{7201,"EXPIRED"},{-6,"CLOCK SKEW"}}){
        weather_reports::Store store;
        check(store.merge(false,payload(false,entry.first),now),"METAR fixture must merge");
        const auto row=store.view(now).front().toMap();
        check(row.value("metarState").toString()==entry.second,"METAR observation age boundary must classify correctly");
        if(!weather_reports::displayable(entry.second))check(row.value("metar").toString().isEmpty()&&row.value("category").toString().isEmpty(),"expired/future METAR must not retain operational text or VFR");
    }
    weather_reports::Store store;
    check(store.merge(true,payload(true,0),now),"TAF-first arrival must merge");
    check(store.merge(false,payload(false,0),now),"later METAR must merge without dropping TAF");
    auto row=store.view(now).front().toMap();
    check(!row.value("metar").toString().isEmpty()&&!row.value("taf").toString().isEmpty(),"independent reports must survive either arrival order");
    check(!store.merge(false,QByteArray("{broken"),now.addSecs(8000)),"malformed response must be rejected");
    row=store.view(now.addSecs(8000)).front().toMap();
    check(row.value("metarState")=="EXPIRED"&&row.value("metar").toString().isEmpty(),"failed fetch must not rejuvenate METAR");
    check(!store.merge(false,payload(false,60),now.addSecs(8000)),"older observation must not replace newer one");
    for(const auto& entry:QList<QPair<qint64,QString>>{{-1,"EXPIRED"},{0,"EXPIRED"},{1,"CURRENT"}}){
        weather_reports::Store taf;
        check(taf.merge(true,payload(true,0,-3600,entry.first),now),"TAF fixture must merge");
        check(taf.view(now).front().toMap().value("tafState")==entry.second,"TAF valid-to boundary is exclusive");
    }
    weather_reports::Store future;
    future.merge(true,payload(true,0,1,3600),now);
    check(future.view(now).front().toMap().value("tafState")=="NOT YET VALID","future TAF window must not be current");
    check(!weather_reports::utcTime(QJsonValue(QStringLiteral("2026-10-21T12:00:00"))).isValid(),"timezone-less timestamp must be rejected");
    check(weather_reports::utcTime(QJsonValue(QStringLiteral("2026-10-21 12:00:00.000Z")))==now,"documented AWC timestamp format must parse");
    check(!weather_reports::utcTime(QJsonValue(1792584000000.0)).isValid(),"millisecond value must not be misread as epoch seconds");
    check(!store.merge(false,QByteArray(512*1024+1,'x'),now),"oversized report must be rejected");
    weather_reports::Store unknown;
    check(unknown.merge(false,QByteArray("[{\"icaoId\":\"LIPH\",\"rawOb\":\"TEST\",\"reportTime\":\"2026-10-21T12:00:00Z\",\"fltCat\":\"VFR\"}]"),now),"missing observation time must be represented");
    check(unknown.view(now).front().toMap().value("metarState")=="UNKNOWN TIME","report/download time must never replace observation time");
    check(unknown.view(now).front().toMap().value("category").toString().isEmpty(),"unknown observation time must not show green VFR");
}
