#include "capture_request.hpp"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QQuickWindow>

#ifdef Q_OS_UNIX
#include <QSocketNotifier>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace efis::ems {

bool capture_window(QQuickWindow* window, const QString& path) {
    if (window == nullptr) return false;
    const QImage image = window->grabWindow();
    if (image.isNull()) return false;
    QDir().mkpath(QFileInfo(path).absolutePath());
    return image.save(path);
}

#ifdef Q_OS_UNIX
namespace {

// Coppia di socket fra il gestore di segnale e il ciclo degli eventi. Dentro un
// gestore di segnale si puo' chiamare pochissimo, e nulla di Qt: scrivere un
// byte e' fra le poche cose lecite, e il QSocketNotifier lo raccoglie nel
// thread della GUI, dove disegnare e salvare e' invece permesso.
int signal_pipe[2] = {-1, -1};

extern "C" void on_capture_signal(int) {
    const char byte = 1;
    // Il valore di ritorno si ignora deliberatamente: dentro un gestore di
    // segnale non c'e' nulla di sensato da fare se la scrittura fallisce, e
    // riprovare rischierebbe di bloccarlo.
    const ssize_t ignored = ::write(signal_pipe[0], &byte, 1);
    static_cast<void>(ignored);
}

}  // namespace

bool install_capture_signal(QQuickWindow* window, const QString& directory) {
    if (window == nullptr) return false;
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signal_pipe) != 0) return false;

    auto* notifier = new QSocketNotifier(signal_pipe[1], QSocketNotifier::Read, window);
    QObject::connect(notifier, &QSocketNotifier::activated, window, [window, directory] {
        char byte = 0;
        const ssize_t read_bytes = ::read(signal_pipe[1], &byte, 1);
        if (read_bytes != 1) return;
        const auto stamp = QDateTime::currentDateTimeUtc().toString(
            QStringLiteral("yyyyMMdd-HHmmsszzz"));
        const auto path = QDir(directory).filePath(
            QStringLiteral("ems-%1.png").arg(stamp));
        const bool saved = capture_window(window, path);
        qInfo().noquote() << "event=capture node=ems trigger=sigusr1 saved=" << saved
                          << "path=" << path;
    });

    struct sigaction action {};
    action.sa_handler = on_capture_signal;
    sigemptyset(&action.sa_mask);
    // SA_RESTART: senza, il segnale interromperebbe le letture in corso altrove
    // nel programma, e una richiesta di schermata non deve avere effetti
    // collaterali su nulla.
    action.sa_flags = SA_RESTART;
    if (::sigaction(SIGUSR1, &action, nullptr) != 0) return false;

    qInfo().noquote() << "event=capture_armed node=ems signal=SIGUSR1 directory="
                      << directory << "pid=" << ::getpid();
    return true;
}

#else

bool install_capture_signal(QQuickWindow*, const QString&) {
    // Windows non ha SIGUSR1. Qui la cattura si ottiene con EFIS_CAPTURE, e non
    // serve altro: il nodo su Windows e' un banco di sviluppo con una finestra
    // vera davanti, non un kiosk senza tastiera.
    return false;
}

#endif

}  // namespace efis::ems
