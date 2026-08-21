#pragma once

#include "efis/instruments/dial.hpp"
#include "efis/instruments/horizon.hpp"

#include <QQuickPaintedItem>
#include <QVariantList>

// Strumento analogico disegnato su schermo.
//
// Non ricalcola nulla: chiede al core la geometria, esattamente come fa il
// renderer SVG che produce le immagini di confronto, e la disegna. Colori e
// spessori vengono da efis/instruments/style.hpp, condiviso con l'altro
// renderer, perche' due rappresentazioni che divergono rendono inutile il
// confronto a immagine.
//
// E' un QQuickPaintedItem e non un nodo scene graph costruito a mano. La scelta
// e' deliberata e ha un prezzo: ogni ridisegno passa da una texture invece che
// da geometria sulla GPU. In cambio testo, archi e ritagli si scrivono una volta
// sola invece di essere ricostruiti a triangoli, e per sei quadranti su un
// pannello e' un prezzo che va misurato, non presunto: se il tempo di fotogramma
// sul Raspberry non regge, il posto dove intervenire e' questo, e la geometria
// nel core non cambia.
class InstrumentItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT

public:
    enum Kind {
        LinearDial,
        Altimeter,
        Heading,
        Attitude,
        TurnAndSlip
    };
    Q_ENUM(Kind)

private:
    Q_PROPERTY(Kind kind READ kind WRITE setKind NOTIFY changed)

    // Lettura principale, e la sua provenienza. La validita' usa i valori di
    // efis::Validity: lo strumento deve poter distinguere un dato assente da uno
    // scaduto, e da uno simulato.
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY changed)
    Q_PROPERTY(int validity READ validity WRITE setValidity NOTIFY changed)
    Q_PROPERTY(bool simulated READ simulated WRITE setSimulated NOTIFY changed)

    // Seconda grandezza: beccheggio sull'orizzonte, scivolata sul
    // virosbandometro, prua selezionata sul girodirezionale.
    Q_PROPERTY(double secondValue READ secondValue WRITE setSecondValue NOTIFY changed)
    Q_PROPERTY(int secondValidity READ secondValidity WRITE setSecondValidity NOTIFY changed)

    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY changed)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY changed)
    Q_PROPERTY(QString windowText READ windowText WRITE setWindowText NOTIFY changed)

    // Scala del quadrante lineare. Ignorata dagli altri tipi, che hanno una
    // geometria propria.
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY changed)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY changed)
    Q_PROPERTY(double majorStep READ majorStep WRITE setMajorStep NOTIFY changed)
    Q_PROPERTY(double minorStep READ minorStep WRITE setMinorStep NOTIFY changed)

    // Fasce, come terne [da, a, banda] con banda 0..3 secondo efis::Band. Vengono
    // dal profilo aeromobile, non dallo strumento.
    Q_PROPERTY(QVariantList bands READ bands WRITE setBands NOTIFY changed)

    // Provenienza dei limiti, secondo efis::LimitsProvenance: 0 assente,
    // 1 indicativa, 2 verificata. Chi passa delle fasce deve dichiarare da dove
    // vengono, e il valore prudente e' quello di partenza.
    Q_PROPERTY(int limitsProvenance READ limitsProvenance WRITE setLimitsProvenance
                   NOTIFY changed)

public:
    explicit InstrumentItem(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    Kind kind() const { return kind_; }
    double value() const { return value_; }
    int validity() const { return validity_; }
    bool simulated() const { return simulated_; }
    double secondValue() const { return second_value_; }
    int secondValidity() const { return second_validity_; }
    QString title() const { return title_; }
    QString unit() const { return unit_; }
    QString windowText() const { return window_text_; }
    double minimum() const { return minimum_; }
    double maximum() const { return maximum_; }
    double majorStep() const { return major_step_; }
    double minorStep() const { return minor_step_; }
    QVariantList bands() const { return bands_; }
    int limitsProvenance() const { return limits_provenance_; }

    void setKind(Kind value);
    void setValue(double value);
    void setValidity(int value);
    void setSimulated(bool value);
    void setSecondValue(double value);
    void setSecondValidity(int value);
    void setTitle(const QString& value);
    void setUnit(const QString& value);
    void setWindowText(const QString& value);
    void setMinimum(double value);
    void setMaximum(double value);
    void setMajorStep(double value);
    void setMinorStep(double value);
    void setBands(const QVariantList& value);
    void setLimitsProvenance(int value);

signals:
    void changed();

private:
    void paintDial(QPainter& painter, double size);
    void paintHorizon(QPainter& painter, double size);
    void paintTurn(QPainter& painter, double size);
    void paintAnnunciation(QPainter& painter, double size,
                           efis::instruments::Annunciation annunciation, bool simulated);
    void paintTitle(QPainter& painter, double size);

    [[nodiscard]] efis::instruments::Reading reading() const;
    [[nodiscard]] efis::instruments::Reading secondReading() const;
    [[nodiscard]] efis::instruments::Sizing sizing(double size) const;

    Kind kind_{LinearDial};
    double value_{};
    int validity_{static_cast<int>(efis::Validity::unavailable)};
    bool simulated_{};
    double second_value_{};
    int second_validity_{static_cast<int>(efis::Validity::unavailable)};
    QString title_;
    QString unit_;
    QString window_text_;
    double minimum_{};
    double maximum_{100.0};
    double major_step_{10.0};
    double minor_step_{2.0};
    QVariantList bands_;
    int limits_provenance_{};
    // Ricavato dal quadrante a ogni disegno, non impostato da fuori.
    mutable bool provisional_limits_{};
};
