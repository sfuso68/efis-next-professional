#pragma once

#include "efis/core/sample.hpp"

#include <chrono>
#include <optional>
#include <span>

namespace efis {

enum class SourceChangeReason : std::uint8_t { initial, failover, reversion };

struct SourceChangeEvent final {
    SourceId previous{SourceId::unknown};
    SourceId selected{SourceId::unknown};
    SourceChangeReason reason{SourceChangeReason::initial};
    MonoTime timestamp{};
};

template <class T>
struct ArbitrationResult final {
    Sample<T> selected;
    std::optional<SourceChangeEvent> change;
};

struct ArbitrationPolicy final {
    std::chrono::milliseconds reversion_dwell{2000};
};

template <class T>
class SourceArbitrator final {
public:
    explicit SourceArbitrator(ArbitrationPolicy policy = {}) : policy_(policy) {}

    [[nodiscard]] ArbitrationResult<T> select(std::span<const Sample<T>> candidates,
                                              MonoTime now) {
        const Sample<T>* preferred = nullptr;
        const Sample<T>* current = nullptr;
        for (const auto& candidate : candidates) {
            if (candidate.source == selected_) current = &candidate;
            if (preferred == nullptr && candidate.usable()) preferred = &candidate;
        }

        if (preferred == nullptr) {
            pending_.reset();
            if (current != nullptr) return {*current, std::nullopt};

            // Nessuna sorgente utilizzabile e nessuna scelta in precedenza.
            //
            // Restituire qui un campione vuoto cancellerebbe l'identita' della
            // grandezza: il consumatore, e con lui la black box, riceverebbe un
            // segnale "sconosciuto" senza unita' invece di "velocita' indicata
            // dal Pixhawk, non utilizzabile". La differenza conta proprio nel
            // caso peggiore, perche' rileggendo un volo si vuole sapere quale
            // grandezza e' caduta, non che ne manca una qualsiasi.
            //
            // Si conserva quindi la provenienza del primo candidato che ne
            // dichiara una, e si forza la validita' a non utilizzabile. Il
            // valore resta quello ricevuto: e' gia' marcato inutilizzabile, e
            // azzerarlo trasformerebbe un dato scartato in uno zero che sembra
            // una misura.
            const Sample<T>* identity = nullptr;
            for (const auto& candidate : candidates) {
                if (candidate.origin.signal != SignalId::unknown) {
                    identity = &candidate;
                    break;
                }
            }
            if (identity == nullptr && !candidates.empty()) {
                identity = &candidates.front();
            }

            Sample<T> unavailable{};
            if (identity != nullptr) unavailable = *identity;
            else unavailable.source = selected_;
            unavailable.validity = Validity::unavailable;
            return {unavailable, std::nullopt};
        }
        if (selected_ == SourceId::unknown) {
            return switch_to(*preferred, SourceChangeReason::initial, now);
        }
        if (current == nullptr || !current->usable()) {
            return switch_to(*preferred, SourceChangeReason::failover, now);
        }
        if (preferred->source == selected_) {
            pending_.reset();
            return {*current, std::nullopt};
        }
        if (!pending_.has_value() || pending_->source != preferred->source) {
            pending_ = Pending{preferred->source, now};
            return {*current, std::nullopt};
        }
        if (now - pending_->since >= policy_.reversion_dwell) {
            return switch_to(*preferred, SourceChangeReason::reversion, now);
        }
        return {*current, std::nullopt};
    }

    [[nodiscard]] SourceId selected_source() const noexcept { return selected_; }

private:
    struct Pending final { SourceId source; MonoTime since; };

    ArbitrationResult<T> switch_to(const Sample<T>& sample,
                                   SourceChangeReason reason,
                                   MonoTime now) {
        const auto previous = selected_;
        selected_ = sample.source;
        pending_.reset();
        return {sample, SourceChangeEvent{previous, selected_, reason, now}};
    }

    ArbitrationPolicy policy_;
    SourceId selected_{SourceId::unknown};
    std::optional<Pending> pending_;
};

}  // namespace efis
