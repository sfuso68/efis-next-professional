#include "connectivity_service.hpp"

#include <QHash>

ConnectivityService::ConnectivityService(QObject* parent) : QObject(parent) {
    connect(&process_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        const auto output = process_.readAllStandardOutput();
        const auto error = process_.readAllStandardError().trimmed();
        if (status != QProcess::NormalExit || exitCode != 0) {
            state_ = QStringLiteral("OFFLINE");
            message_ = error.isEmpty() ? QStringLiteral("NetworkManager command failed") : QString::fromUtf8(error);
        } else if (operation_ == QStringLiteral("status")) {
            parseStatus(output);
            run({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("IN-USE,SSID,SIGNAL,SECURITY"),
                 QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("list"), QStringLiteral("--rescan"), QStringLiteral("yes")}, QStringLiteral("scan"));
            return;
        } else if (operation_ == QStringLiteral("scan")) {
            parseScan(output);
        } else if (operation_ == QStringLiteral("activate")) {
            message_ = QStringLiteral("Saved hotspot profile activated");
            refresh();
            return;
        }
        emit changed();
    });
    timer_.setInterval(15000);
    connect(&timer_, &QTimer::timeout, this, &ConnectivityService::refresh);
    timer_.start();
    QTimer::singleShot(0, this, &ConnectivityService::refresh);
}

void ConnectivityService::run(const QStringList& arguments, const QString& operation) {
    if (busy()) return;
    operation_ = operation;
    process_.start(QStringLiteral("nmcli"), arguments);
    emit changed();
}

void ConnectivityService::refresh() {
    run({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("STATE,CONNECTIVITY,WIFI-HW,WIFI"), QStringLiteral("general")}, QStringLiteral("status"));
}

bool ConnectivityService::activateSavedProfile(const QString& profile, bool groundConfirmed) {
    if (!groundConfirmed) { message_ = QStringLiteral("Network changes are inhibited in flight"); emit changed(); return false; }
    if (profile.trimmed().isEmpty() || busy()) return false;
    // Only a saved NetworkManager profile name is passed. No secret enters argv, QML or logs.
    run({QStringLiteral("connection"), QStringLiteral("up"), QStringLiteral("id"), profile}, QStringLiteral("activate"));
    message_ = QStringLiteral("Activating saved profile %1").arg(profile);
    return true;
}

void ConnectivityService::parseStatus(const QByteArray& output) {
    const auto fields = QString::fromUtf8(output).trimmed().split(':');
    state_ = fields.value(0).toUpper();
    if (fields.value(1).contains(QStringLiteral("full"), Qt::CaseInsensitive)) state_ = QStringLiteral("ONLINE");
    message_ = QStringLiteral("NetworkManager • %1").arg(state_);
}

void ConnectivityService::parseScan(const QByteArray& output) {
    QVariantList result;
    QHash<QString, int> positions;
    active_ssid_ = QStringLiteral("—");
    for (const auto& line : QString::fromUtf8(output).split('\n', Qt::SkipEmptyParts)) {
        const auto fields = line.split(':');
        const auto ssid = fields.value(1).trimmed();
        if (ssid.isEmpty()) continue;
        const bool active = fields.value(0) == QStringLiteral("*");
        if (active) active_ssid_ = ssid;
        QVariantMap entry{{QStringLiteral("ssid"), ssid}, {QStringLiteral("signal"), fields.value(2).toInt()},
            {QStringLiteral("security"), fields.value(3).isEmpty() ? QStringLiteral("OPEN") : fields.value(3)},
            {QStringLiteral("active"), active}};
        if (positions.contains(ssid)) {
            const int position = positions.value(ssid);
            const auto previous = result.at(position).toMap();
            if (active || entry.value(QStringLiteral("signal")).toInt() > previous.value(QStringLiteral("signal")).toInt())
                result[position] = entry;
        } else {
            positions.insert(ssid, result.size());
            result.push_back(entry);
        }
    }
    networks_ = result;
    if (active_ssid_ != QStringLiteral("—")) message_ = QStringLiteral("Connected to %1").arg(active_ssid_);
}
