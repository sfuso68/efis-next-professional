#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

class ConnectivityService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString activeSsid READ activeSsid NOTIFY changed)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QVariantList networks READ networks NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit ConnectivityService(QObject* parent = nullptr);
    [[nodiscard]] QString state() const { return state_; }
    [[nodiscard]] QString activeSsid() const { return active_ssid_; }
    [[nodiscard]] QString ipAddress() const { return ip_address_; }
    [[nodiscard]] QString message() const { return message_; }
    [[nodiscard]] QVariantList networks() const { return networks_; }
    [[nodiscard]] bool busy() const { return process_.state() != QProcess::NotRunning; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool activateSavedProfile(const QString& profile, bool groundConfirmed);
signals:
    void changed();
private:
    void run(const QStringList& arguments, const QString& operation);
    void parseStatus(const QByteArray& output);
    void parseScan(const QByteArray& output);
    QProcess process_;
    QTimer timer_;
    QString operation_;
    QString state_{QStringLiteral("CHECKING")};
    QString active_ssid_{QStringLiteral("—")};
    QString ip_address_{QStringLiteral("—")};
    QString message_{QStringLiteral("NetworkManager status pending")};
    QVariantList networks_;
};
