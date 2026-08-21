#include "efis/io/socketcan_source.hpp"

#include <cstring>

#if defined(__linux__)
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace efis::io {

#if defined(__linux__)

bool SocketCanSource::supported() noexcept { return true; }

SocketCanSource::SocketCanSource(std::string interface_name)
    : interface_(std::move(interface_name)) {}

SocketCanSource::~SocketCanSource() { close(); }

bool SocketCanSource::open() {
    close();
    error_.clear();

    const int socket_descriptor = ::socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);
    if (socket_descriptor < 0) {
        error_ = std::string{"socket CAN non creato: "} + std::strerror(errno);
        return false;
    }

    ifreq request{};
    // Il nome dell'interfaccia va troncato in modo sicuro: ifr_name ha
    // dimensione fissa e un nome piu' lungo non e' un errore da ignorare.
    if (interface_.size() >= IFNAMSIZ) {
        ::close(socket_descriptor);
        error_ = "nome dell'interfaccia troppo lungo: " + interface_;
        return false;
    }
    std::memcpy(request.ifr_name, interface_.c_str(), interface_.size() + 1);

    if (::ioctl(socket_descriptor, SIOCGIFINDEX, &request) < 0) {
        ::close(socket_descriptor);
        // Il caso piu' comune: l'interfaccia non esiste, oppure esiste e non e'
        // stata portata su. Dirlo e' l'intero motivo per cui open() puo'
        // fallire invece di restituire semplicemente zero frame.
        error_ = "interfaccia " + interface_ + " non trovata: " + std::strerror(errno);
        return false;
    }

    // Chiedere al nucleo di segnalare i frame di errore. Senza questo un bus che
    // cede resta indistinguibile da un bus fermo.
    can_err_mask_t error_mask = CAN_ERR_MASK;
    ::setsockopt(socket_descriptor, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &error_mask,
                 sizeof(error_mask));

    // Contatore dei frame persi dal nucleo. Se la coda si riempie vogliamo
    // saperlo: un buco nei dati che nessuno dichiara e' peggio di un buco
    // dichiarato.
    int receive_own = 0;
    ::setsockopt(socket_descriptor, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
                 &receive_own, sizeof(receive_own));
    int drop_monitor = 1;
    ::setsockopt(socket_descriptor, SOL_SOCKET, SO_RXQ_OVFL, &drop_monitor,
                 sizeof(drop_monitor));

    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = request.ifr_ifindex;

    if (::bind(socket_descriptor, reinterpret_cast<sockaddr*>(&address),
               sizeof(address)) < 0) {
        ::close(socket_descriptor);
        error_ = "bind su " + interface_ + " fallito: " + std::strerror(errno);
        return false;
    }

    descriptor_ = socket_descriptor;
    return true;
}

void SocketCanSource::close() noexcept {
    if (descriptor_ >= 0) {
        ::close(descriptor_);
        descriptor_ = -1;
    }
}

std::size_t SocketCanSource::read(std::span<can::Frame> frames) {
    if (!is_open()) return 0;

    std::size_t produced = 0;
    while (produced < frames.size()) {
        can_frame raw{};
        const ssize_t received = ::read(descriptor_, &raw, sizeof(raw));
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // per ora niente
            if (errno == EINTR) continue;
            ++statistics_.read_errors;
            break;
        }
        if (received < static_cast<ssize_t>(sizeof(can_frame))) {
            // Lettura parziale: non e' un frame, e completarlo sarebbe inventare.
            ++statistics_.read_errors;
            continue;
        }

        // Un frame di errore descrive lo stato del bus, non una grandezza
        // fisica. Consegnarlo all'adapter significherebbe leggere un guasto del
        // cavo come se fosse un assetto.
        if ((raw.can_id & CAN_ERR_FLAG) != 0) {
            ++statistics_.error_frames;
            continue;
        }
        if ((raw.can_id & CAN_RTR_FLAG) != 0) {
            ++statistics_.remote_frames;
            continue;
        }

        can::Frame frame{};
        frame.id = (raw.can_id & CAN_EFF_FLAG) != 0
                       ? (raw.can_id & CAN_EFF_MASK)
                       : (raw.can_id & CAN_SFF_MASK);
        frame.length = raw.can_dlc > 8 ? std::uint8_t{8} : raw.can_dlc;
        std::memcpy(frame.data.data(), raw.data, frame.length);

        frames[produced] = frame;
        ++produced;
        ++statistics_.frames_read;
    }

    return produced;
}

#else  // Piattaforme senza SocketCAN

bool SocketCanSource::supported() noexcept { return false; }

SocketCanSource::SocketCanSource(std::string interface_name)
    : interface_(std::move(interface_name)) {}

SocketCanSource::~SocketCanSource() = default;

bool SocketCanSource::open() {
    // Fallisce dichiarando il motivo. Restituire zero frame in silenzio farebbe
    // apparire un bus muto dove non c'e' alcun bus, ed e' la diagnosi sbagliata.
    error_ = "SocketCAN non disponibile su questa piattaforma";
    return false;
}

void SocketCanSource::close() noexcept {}

std::size_t SocketCanSource::read(std::span<can::Frame>) { return 0; }

#endif

}  // namespace efis::io
