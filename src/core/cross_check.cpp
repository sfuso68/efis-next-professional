#include "efis/core/cross_check.hpp"

#include <cmath>

namespace efis {

CrossCheck::CrossCheck(CrossCheckPolicy policy) noexcept : policy_(policy) {}

CrossCheckResult CrossCheck::compare(const Sample<double>& first,
                                     const Sample<double>& second,
                                     MonoTime now) {
    if (!first.usable() || !second.usable()) {
        exceeding_ = false;
        result_ = {};
        return result_;
    }

    const double difference = std::abs(first.value - second.value);
    result_.difference = difference;

    if (difference <= policy_.tolerance) {
        exceeding_ = false;
        result_.exceeded_for = std::chrono::milliseconds::zero();
        result_.agreement = Agreement::agree;
        return result_;
    }

    if (!exceeding_) {
        exceeding_ = true;
        exceeded_since_ = now;
    }

    const auto exceeded = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - exceeded_since_);
    result_.exceeded_for = exceeded;

    // Finche' non e' trascorso il tempo di persistenza lo scostamento c'e' ma
    // non e' ancora una discrepanza: si dichiara accordo, non ignoto, perche'
    // uno scostamento transitorio entro la finestra prevista e' comportamento
    // normale e non una mancanza di informazione.
    result_.agreement = exceeded >= policy_.dwell ? Agreement::disagree : Agreement::agree;
    return result_;
}

void CrossCheck::reset() noexcept {
    exceeding_ = false;
    result_ = {};
}

void apply_disagreement(const CrossCheckResult& result,
                        Sample<double>& first,
                        Sample<double>& second) noexcept {
    if (result.agreement != Agreement::disagree) return;

    // Si marcano entrambi. Il confronto stabilisce che le due catene non
    // concordano, non quale abbia torto: marcarne una sola presenterebbe
    // l'altra come sana, cioe' esattamente l'informazione che non si possiede.
    first.validity = Validity::disagree;
    second.validity = Validity::disagree;
}

}  // namespace efis
