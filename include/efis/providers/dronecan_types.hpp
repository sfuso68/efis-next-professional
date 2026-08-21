#pragma once

#include <cstdint>

namespace efis::dronecan {

// Forma di un campo DSDL sul filo.
//
// Il DSDL non allinea i campi ai byte: un `uint2` occupa due bit e quello dopo
// comincia dal terzo. Per questo ogni descrittore porta la propria ampiezza in
// bit e i campi vanno letti in sequenza, non a offset fissi.
enum class FieldShape : std::uint8_t {
    // Riempimento dichiarato dal tipo. Non ha un valore: va saltato, e saltarlo
    // e' obbligatorio perche' i campi che seguono cominciano dopo.
    padding,

    boolean,
    unsigned_integer,
    signed_integer,

    // Virgola mobile a 16, 32 o 64 bit secondo l'ampiezza dichiarata. Il formato
    // a 16 bit e' quello di IEEE 754 half precision, e non esiste come tipo
    // nativo in C++: va convertito a mano.
    floating,

    // Array. Se dinamico, sul filo lo precede il numero di elementi, su tanti
    // bit quanti ne servono a contare fino al massimo dichiarato.
    array,

    // Struttura annidata, descritta da un tipo proprio nella tabella.
    compound
};

struct FieldDescriptor final {
    const char* name;
    FieldShape shape;

    // Ampiezza in bit del campo. Zero per array e strutture, che non hanno una
    // dimensione propria ma quella di cio' che contengono.
    std::uint8_t bits;

    // Solo per gli array.
    std::uint8_t max_size{};
    bool dynamic{};
};

struct TypeDescriptor final {
    // Identificativo del tipo di dato. Zero per i tipi annidati, che non
    // viaggiano da soli e quindi non ne hanno uno.
    std::uint32_t data_type_id;

    const char* name;

    // Entra nel CRC del trasferimento. Un valore sbagliato non produce un
    // errore visibile: fa scartare in silenzio tutti i trasferimenti di quel
    // tipo, come un CRC_EXTRA sbagliato in MAVLink.
    std::uint64_t signature;

    const FieldDescriptor* fields;
    std::uint8_t field_count;
};

}  // namespace efis::dronecan
