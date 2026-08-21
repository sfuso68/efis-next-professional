#pragma once

#include "efis/core/types.hpp"

namespace efis {

// Versione dello schema del campione. Accompagna il campione quando viene
// serializzato verso la black box o verso il Display Data Protocol, dove
// l'envelope dichiara gia' un campo `schema`. Un consumer che riceve una
// versione maggiore di quella che conosce deve rifiutare il messaggio, non
// interpretarlo parzialmente.
inline constexpr std::uint16_t sample_schema_version = 2;

// Provenienza del campione: da dove viene, che cosa rappresenta e in quale
// unita' e' espresso.
//
// Il paragrafo 17.2 dell'handoff elenca i campi che ogni campione deve portare
// con se' per essere registrabile nella black box. Sono raccolti qui invece che
// direttamente in Sample per due motivi: restano raggruppati come un'unita'
// concettuale, e Sample continua a essere inizializzabile per posizione dal
// codice gia' esistente, che ne valorizza solo i primi campi.
struct SampleOrigin final {
    SignalId signal{SignalId::unknown};
    NodeId node{node_undeclared};
    ChannelId channel{channel_undeclared};
    Unit unit{Unit::unknown};

    // Revisione del profilo di calibrazione applicato per ottenere il valore
    // ingegneristico dal valore grezzo. Serve a sapere, rileggendo una
    // registrazione, con quale taratura quel numero e' stato prodotto: senza
    // questo dato una ricalibrazione rende i log passati non confrontabili.
    std::uint32_t calibration_revision{};

    // Marcatore esplicito di dato simulato, indipendente da Validity.
    //
    // Validity puo' assumere un solo stato per volta: un campione simulato che
    // invecchia diventa `stale` e perde ogni traccia di essere simulato. Il
    // paragrafo 19 dell'handoff vieta che un valore simulato possa presentarsi
    // come reale, quindi il marcatore deve sopravvivere a qualsiasi transizione
    // di validita'.
    bool simulated{};
};

template <class T>
struct Sample final {
    // Valore ingegneristico, gia' convertito nell'unita' dichiarata da
    // origin.unit.
    T value{};
    SourceId source{SourceId::unknown};
    MonoTime sampled_at{};
    MonoTime received_at{};
    Quality quality{Quality::unknown};
    Validity validity{Validity::unavailable};
    std::uint64_t sequence{};

    SampleOrigin origin{};

    // Valore grezzo prima della conversione: conteggi ADC, unita' native del
    // bus, pressione differenziale non compensata. Conservarlo permette di
    // ricalcolare il valore ingegneristico con una taratura diversa senza
    // ripetere la prova, e di distinguere un sensore fermo da una conversione
    // sbagliata.
    double raw{};

    // Tempo UTC del campionamento, se il nodo dispone di una sorgente di tempo
    // affidabile. Vale utc_unavailable quando non lo e'.
    UtcMicros utc{utc_unavailable};

    [[nodiscard]] constexpr bool usable() const noexcept {
        return is_usable(validity);
    }

    // Vero se il campione non descrive il mondo reale. Considera sia il
    // marcatore esplicito sia lo stato di validita', cosi' il codice gia'
    // scritto che imposta solo Validity::simulated resta corretto.
    [[nodiscard]] constexpr bool is_simulated() const noexcept {
        return origin.simulated || validity == Validity::simulated;
    }

    [[nodiscard]] constexpr bool has_utc() const noexcept {
        return utc != utc_unavailable;
    }
};

}  // namespace efis
