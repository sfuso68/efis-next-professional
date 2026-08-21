#pragma once

#include "efis/core/types.hpp"

#include <string>
#include <vector>

namespace efis::instruments {

// Geometria di uno strumento circolare, indipendente da come verra' disegnata.
//
// Vive nel core e non conosce Qt. Due consumatori la useranno: il nodo scene
// graph che disegna sullo schermo e il renderer SVG di riferimento che produce
// le immagini di confronto. Avere una sola descrizione per entrambi e' cio' che
// rende gli strumenti verificabili senza una scheda grafica, e quindi anche su
// una macchina dove i binari non si possono nemmeno eseguire.
//
// Le coordinate sono normalizzate: origine al centro del quadrante, raggio
// unitario. La stessa descrizione serve un display circolare da 240 px e un
// pannello da 1024, e la dimensione entra soltanto dove cambia davvero le
// scelte, cioe' in quante tacche e quante scritte ci stanno.
//
// Gli angoli sono in gradi, con lo zero a ore dodici e il verso orario
// positivo, che e' la convenzione dei quadranti aeronautici e non quella
// trigonometrica.

struct Point final {
    double x{};
    double y{};
};

// Converte un angolo del quadrante in un punto sulla circonferenza di raggio
// dato. Serve al renderer, ed e' qui perche' la convenzione degli angoli non
// deve essere reinterpretata da chi disegna.
[[nodiscard]] Point polar(double angle_deg, double radius) noexcept;

enum class TickKind : std::uint8_t {
    // Tacca lunga, quella che porta la scritta.
    major,
    // Tacca corta di suddivisione.
    minor
};

struct Tick final {
    double angle_deg{};
    double inner_radius{};
    double outer_radius{};
    TickKind kind{TickKind::minor};
};

struct Label final {
    double angle_deg{};
    double radius{};

    // Altezza del carattere in frazione del raggio, cosi' resta proporzionata a
    // qualunque dimensione.
    double height{};

    std::string text;
};

// Fascia colorata sul quadrante: gli archi di velocita' e i limiti motore.
//
// Il significato e' quello aeronautico e non e' decorativo: il verde e' la
// gamma di funzionamento normale, il giallo la gamma da usare con cautela, il
// rosso il limite da non superare.
enum class Band : std::uint8_t { white, green, yellow, red };

struct Arc final {
    double start_deg{};
    double end_deg{};
    double inner_radius{};
    double outer_radius{};
    Band band{Band::green};
};

enum class NeedleKind : std::uint8_t {
    // Lancetta principale: la piu' lunga e la piu' sottile. Sull'altimetro e'
    // quella delle centinaia, che compie un giro ogni mille piedi.
    primary,
    // Lancetta secondaria: piu' corta e piu' larga. Le migliaia dell'altimetro,
    // il secondo serbatoio su un indicatore bi-ago.
    secondary,
    // Lancetta terziaria: le decine di migliaia dell'altimetro, disegnata come
    // un indice corto vicino al centro.
    //
    // La gerarchia non e' estetica. Su un altimetro a tre lancette la lettura
    // errata di mille piedi e' l'incidente classico, e nasce dal confondere due
    // lancette simili: sono deliberatamente diverse per lunghezza e spessore.
    tertiary,

    // Indice fisso: non indica un valore misurato ma il punto del quadrante
    // rispetto a cui si legge. Sul girodirezionale e' la linea di fede a ore
    // dodici, che resta ferma mentre la rosa gira.
    reference,

    // Indice di un valore scelto dal pilota: la prua selezionata, una velocita'
    // di riferimento. Va distinto dagli altri perche' non e' una misura e non
    // deve sparire quando la misura manca.
    bug
};

struct Needle final {
    double angle_deg{};
    double length{};

    // Sporgenza dalla parte opposta al centro, come nelle lancette vere.
    double tail{};

    NeedleKind kind{NeedleKind::primary};
};

// Stato dichiarato dallo strumento, mostrato al posto della lettura.
//
// Un quadrante guasto non deve mostrare una lancetta ferma su zero: zero e' una
// lettura possibile, e un osservatore la leggerebbe come una misura. Quando lo
// strumento non ha un valore da mostrare, la lancetta sparisce e resta la
// bandierina. E' l'idioma degli strumenti veri, ed e' anche quello onesto.
enum class Annunciation : std::uint8_t {
    // Nessuna dicitura: la lettura e' valida e attuale.
    none,
    // Dato non disponibile.
    failed,
    // Dato scaduto: c'era, non e' piu' attuale.
    stale,
    // Valore fuori dai limiti della scala.
    out_of_range
};

[[nodiscard]] constexpr const char* annunciation_text(Annunciation value) noexcept {
    switch (value) {
        case Annunciation::none: return "";
        case Annunciation::failed: return "FAIL";
        case Annunciation::stale: return "STALE";
        case Annunciation::out_of_range: return "RANGE";
    }
    return "FAIL";
}

// Finestrella con un valore scritto, non indicato da una lancetta.
struct Window final {
    bool present{};
    double angle_deg{};
    double radius{};
    double height{};
    std::string text;
};

struct Dial final {
    std::vector<Tick> ticks;
    std::vector<Label> labels;
    std::vector<Arc> arcs;

    // Vuoto quando lo strumento non ha un valore da mostrare. Vedi
    // Annunciation: una lancetta assente e' un'informazione, una lancetta su
    // zero e' una bugia.
    std::vector<Needle> needles;

    // Finestrella sul quadrante: la regolazione barometrica dell'altimetro, e in
    // futuro il cilindro selezionato o il serbatoio attivo. Il testo e' gia'
    // formattato perche' la scelta dell'unita' e' del profilo, non dello
    // strumento.
    Window window;

    Annunciation annunciation{Annunciation::none};

    // Vero se il valore mostrato non descrive il mondo reale. La marcatura
    // vera e propria e' compito della pagina, ma lo strumento deve dichiararlo
    // perche' su una pagina mista non tutti i quadranti hanno la stessa
    // provenienza.
    bool simulated{};
};

// Definizione della scala di un quadrante.
//
// Copre gli strumenti a scala lineare, che sono la maggioranza: anemometro,
// contagiri, pressioni, temperature, quantita', tensione e corrente. Assetto e
// bussola hanno geometrie proprie e non passano di qui.
struct Scale final {
    double minimum{};
    double maximum{100.0};

    // Angolo della prima tacca e ampiezza percorsa dalla lancetta. Un quadrante
    // aperto in basso usa per esempio start -150 e sweep 300.
    double start_deg{-150.0};
    double sweep_deg{300.0};

    // Passo fra le tacche lunghe e fra quelle corte, nelle unita' della
    // grandezza.
    double major_step{10.0};
    double minor_step{2.0};

    // Cifre decimali delle scritte.
    int decimals{};

    // Ampiezza dopo la quale la lancetta ricomincia il giro. Zero significa
    // nessun avvolgimento. Serve alla lancetta delle centinaia dell'altimetro,
    // che compie un giro ogni mille piedi.
    double wrap{};
};

// Fascia espressa nelle unita' della grandezza, come arriva dal profilo
// aeromobile o dai limiti motore.
struct BandRange final {
    double from{};
    double to{};
    Band band{Band::green};
};

// Cio' che si sa del valore da mostrare.
struct Reading final {
    double value{};
    Validity validity{Validity::unavailable};
    bool simulated{};
};

// Dimensione fisica alla quale il quadrante verra' disegnato.
//
// Entra nella descrizione soltanto per decidere quante tacche e quante scritte
// ci stanno: sotto una certa distanza fra due tacche l'occhio non le separa
// piu', e disegnarle rende il quadrante peggiore, non piu' preciso.
struct Sizing final {
    double diameter_px{480.0};

    // Distanza minima fra due tacche corte perche' restino distinguibili.
    double minimum_tick_spacing_px{4.0};

    // Distanza minima fra due scritte perche' restino leggibili.
    double minimum_label_spacing_px{28.0};
};

// Costruisce la geometria del quadrante.
//
// Le fasce vengono ritagliate sui limiti della scala: una fascia che sborda
// verrebbe disegnata fuori dal quadrante, e una che cade interamente fuori non
// viene disegnata affatto.
[[nodiscard]] Dial describe(const Scale& scale, const Reading& reading,
                            const Sizing& sizing,
                            const std::vector<BandRange>& bands = {});

// Angolo corrispondente a un valore sulla scala.
//
// Il valore viene limitato agli estremi: una lancetta che superasse il fondo
// scala tornerebbe indietro e mostrerebbe una lettura bassa credibile al posto
// di una alta impossibile. Il superamento si dichiara con l'annuncio, non
// facendo girare la lancetta.
[[nodiscard]] double angle_for(const Scale& scale, double value) noexcept;

// --- Altimetro a tre lancette ------------------------------------------------
//
// Ha una funzione propria perche' non e' un quadrante a scala lineare: tre
// lancette leggono la stessa grandezza su tre decadi diverse, e ciascuna compie
// un giro completo quando la precedente ne compie un decimo.
//
// La lettura errata di mille piedi e' l'incidente classico dell'altimetro a tre
// lancette, e nasce dal confondere la lancetta delle migliaia con quella delle
// decine di migliaia. Le tre sono percio' deliberatamente diverse per lunghezza
// e spessore, e la piu' lunga e' quella che si muove piu' in fretta, come sugli
// strumenti veri.
//
// La finestrella riporta la regolazione gia' formattata: l'unita' la sceglie il
// pilota nel profilo, e lo strumento non deve saperne nulla.
struct AltimeterReading final {
    // Quota indicata, cioe' gia' corretta per la regolazione.
    Reading altitude;

    // Testo della finestrella, per esempio "1013" oppure "29.92". Vuoto se la
    // regolazione non e' disponibile: una finestrella vuota dichiara che la
    // regolazione manca, una che mostra 1013 direbbe che e' standard.
    std::string setting_text;
};

[[nodiscard]] Dial describe_altimeter(const AltimeterReading& reading,
                                      const Sizing& sizing);

// --- Girodirezionale ---------------------------------------------------------
//
// Qui non ruota la lancetta, ruota la rosa: la prua corrente si legge sotto la
// linea di fede fissa a ore dodici, e la graduazione scorre sotto di essa. E' la
// ragione per cui non passa da describe(), dove sono le tacche a stare ferme.
//
// Quando la prua manca la rosa non viene disegnata affatto. Una rosa ferma
// sarebbe la rappresentazione esatta di un girodirezionale guasto, e verrebbe
// letta come una prua: e' il guasto che si vuole rendere impossibile da
// scambiare per una misura, non da riprodurre fedelmente.
struct HeadingReading final {
    Reading heading;

    // Prua selezionata. Non e' una misura, ma la sua posizione sul quadrante e'
    // definita rispetto alla rosa: senza prua corrente non c'e' un posto dove
    // metterla, e sottrarre una prua che non esiste darebbe un angolo privo di
    // significato. Cio' che il pilota ha scelto non e' perduto, semplicemente
    // questo strumento non puo' piu' mostrarlo.
    Reading bug;
};

[[nodiscard]] Dial describe_heading(const HeadingReading& reading,
                                    const Sizing& sizing);

// Riporta un angolo nell'intervallo [0, 360). Le prue si avvolgono: 370 gradi
// sono 10, non un valore fuori scala da limitare.
[[nodiscard]] double normalise_heading(double heading_deg) noexcept;

}  // namespace efis::instruments
