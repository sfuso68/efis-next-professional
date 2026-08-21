#pragma once

#include <cstdint>

namespace efis::instruments::style {

// Costanti grafiche degli strumenti, condivise dai due renderer.
//
// La geometria sta gia' nel core e nessuno dei due la ricalcola. Restavano pero'
// i colori e gli spessori, scritti due volte: il renderer SVG produce le
// immagini di confronto, quello Qt disegna sul vetro, e se i due divergessero il
// confronto a immagine smetterebbe di dire qualcosa sullo strumento vero. Stanno
// percio' qui, in un punto solo.
//
// I colori sono quelli del design system. Rosso e ambra restano esclusivi degli
// allarmi e degli indici: non compaiono altrove sul quadrante, cosi' quando
// compaiono significano qualcosa.

inline constexpr std::uint32_t background = 0x071019;
inline constexpr std::uint32_t face = 0x0B1722;
inline constexpr std::uint32_t bezel = 0x314654;
inline constexpr std::uint32_t primary = 0xF4F7FA;
inline constexpr std::uint32_t secondary = 0x9AAEBD;
inline constexpr std::uint32_t cyan = 0x00C8F0;
inline constexpr std::uint32_t green = 0x58D330;
inline constexpr std::uint32_t amber = 0xFFB000;
inline constexpr std::uint32_t warning = 0xFF3B30;
inline constexpr std::uint32_t sky = 0x087AC1;
inline constexpr std::uint32_t earth = 0x795126;

// Spessori, espressi in frazione del lato dello strumento: uno strumento piu'
// grande ha tratti proporzionalmente uguali, non piu' sottili.
inline constexpr double bezel_width = 0.006;
inline constexpr double major_tick_width = 0.010;
inline constexpr double minor_tick_width = 0.005;
inline constexpr double horizon_line_width = 0.008;
inline constexpr double rung_width = 0.005;
inline constexpr double aircraft_width = 0.014;
inline constexpr double simulation_frame_width = 0.012;

// Spessore delle lancette per tipo. La differenza non e' estetica: sull
// altimetro a tre lancette la lettura errata di mille piedi nasce dal
// confonderne due, e lo spessore e' cio' che le distingue.
inline constexpr double primary_needle_width = 0.014;
inline constexpr double secondary_needle_width = 0.026;
inline constexpr double tertiary_needle_width = 0.034;
inline constexpr double reference_needle_width = 0.012;
inline constexpr double bug_needle_width = 0.020;

// Raggio del quadrante in frazione del lato, e proporzioni delle scritte.
inline constexpr double dial_radius = 0.42;
inline constexpr double bezel_radius = 1.06;
inline constexpr double hub_radius = 0.022;
inline constexpr double title_height = 0.13;
inline constexpr double unit_height = 0.11;
inline constexpr double annunciation_height = 0.15;

}  // namespace efis::instruments::style
