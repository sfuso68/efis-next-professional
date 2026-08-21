#include "instrument_item.hpp"

#include "efis/instruments/style.hpp"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

using namespace efis;
using namespace efis::instruments;

namespace {

QColor colour(std::uint32_t rgb) {
    return QColor::fromRgb(static_cast<QRgb>(0xFF000000U | rgb));
}

// Il core lavora con lo zero a ore dodici e verso orario; Qt con lo zero a ore
// tre e verso antiorario. La conversione sta qui e non sparsa nel disegno,
// perche' un solo punto in cui sbagliarla e' molto meglio di dieci.
QPointF at(double centre, double angle_deg, double radius) {
    const Point point = polar(angle_deg, radius);
    return QPointF{centre + point.x, centre + point.y};
}

QColor band_colour(Band band) {
    switch (band) {
        case Band::white: return colour(style::primary);
        case Band::green: return colour(style::green);
        case Band::yellow: return colour(style::amber);
        case Band::red: return colour(style::warning);
    }
    return colour(style::primary);
}

QColor needle_colour(NeedleKind kind) {
    switch (kind) {
        case NeedleKind::reference: return colour(style::amber);
        case NeedleKind::bug: return colour(style::cyan);
        default: return colour(style::primary);
    }
}

double needle_width(NeedleKind kind) {
    switch (kind) {
        case NeedleKind::primary: return style::primary_needle_width;
        case NeedleKind::secondary: return style::secondary_needle_width;
        case NeedleKind::tertiary: return style::tertiary_needle_width;
        case NeedleKind::reference: return style::reference_needle_width;
        case NeedleKind::bug: return style::bug_needle_width;
    }
    return style::primary_needle_width;
}

void draw_text(QPainter& painter, const QPointF& centre, double height,
               const QString& text, const QColor& text_colour, bool bold = true) {
    QFont font = painter.font();
    font.setPixelSize(std::max(1, static_cast<int>(height)));
    font.setBold(bold);
    painter.setFont(font);
    painter.setPen(text_colour);
    const QRectF box{centre.x() - height * 4.0, centre.y() - height,
                     height * 8.0, height * 2.0};
    painter.drawText(box, Qt::AlignCenter, text);
}

}  // namespace

InstrumentItem::InstrumentItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &InstrumentItem::changed, this, [this] { update(); });
}

Reading InstrumentItem::reading() const {
    return Reading{value_, static_cast<Validity>(validity_), simulated_};
}

Reading InstrumentItem::secondReading() const {
    return Reading{second_value_, static_cast<Validity>(second_validity_), simulated_};
}

Sizing InstrumentItem::sizing(double size) const {
    Sizing result;
    result.diameter_px = size;
    return result;
}

void InstrumentItem::paint(QPainter* painter) {
    const double size = std::min(width(), height());
    if (size <= 0.0) return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(QRectF{0, 0, width(), height()}, colour(style::background));

    switch (kind_) {
        case Attitude: paintHorizon(*painter, size); break;
        case TurnAndSlip: paintTurn(*painter, size); break;
        default: paintDial(*painter, size); break;
    }
}

void InstrumentItem::paintDial(QPainter& painter, double size) {
    const double centre = size / 2.0;
    const double radius = size * style::dial_radius;

    Dial dial;
    if (kind_ == Altimeter) {
        dial = describe_altimeter({reading(), window_text_.toStdString()}, sizing(size));
    } else if (kind_ == Heading) {
        dial = describe_heading({reading(), secondReading()}, sizing(size));
    } else {
        std::vector<BandRange> ranges;
        for (const QVariant& entry : bands_) {
            const QVariantList triple = entry.toList();
            if (triple.size() != 3) continue;
            ranges.push_back(BandRange{triple[0].toDouble(), triple[1].toDouble(),
                                       static_cast<Band>(triple[2].toInt())});
        }
        const Scale scale{minimum_, maximum_, -150.0, 300.0, major_step_, minor_step_, 0, 0.0};
        dial = describe(scale, reading(), sizing(size), ranges);
    }

    painter.setPen(QPen{colour(style::bezel), size * style::bezel_width});
    painter.setBrush(colour(style::face));
    painter.drawEllipse(QPointF{centre, centre}, radius * style::bezel_radius,
                        radius * style::bezel_radius);
    painter.setBrush(Qt::NoBrush);

    for (const Arc& arc : dial.arcs) {
        QPainterPath path;
        const double outer = radius * arc.outer_radius;
        const double inner = radius * arc.inner_radius;
        // Qt misura gli angoli in sedicesimi di grado, dallo zero a ore tre e in
        // verso antiorario: da qui la conversione.
        const double start = -(arc.start_deg - 90.0);
        const double span = -(arc.end_deg - arc.start_deg);
        path.arcMoveTo(QRectF{centre - outer, centre - outer, outer * 2, outer * 2}, start);
        path.arcTo(QRectF{centre - outer, centre - outer, outer * 2, outer * 2}, start, span);
        path.arcTo(QRectF{centre - inner, centre - inner, inner * 2, inner * 2},
                   start + span, -span);
        path.closeSubpath();
        painter.fillPath(path, band_colour(arc.band));
    }

    for (const Tick& tick : dial.ticks) {
        const double width = size * (tick.kind == TickKind::major ? style::major_tick_width
                                                                  : style::minor_tick_width);
        painter.setPen(QPen{colour(style::primary), width, Qt::SolidLine, Qt::RoundCap});
        painter.drawLine(at(centre, tick.angle_deg, radius * tick.inner_radius),
                         at(centre, tick.angle_deg, radius * tick.outer_radius));
    }

    for (const Label& label : dial.labels) {
        draw_text(painter, at(centre, label.angle_deg, radius * label.radius),
                  radius * label.height, QString::fromStdString(label.text),
                  colour(style::primary));
    }

    if (dial.window.present) {
        const QPointF where = at(centre, dial.window.angle_deg, radius * dial.window.radius);
        const double height = radius * dial.window.height;
        const double box_width =
            height * (0.72 * static_cast<double>(dial.window.text.size()) + 0.8);
        painter.setPen(QPen{colour(style::bezel), size * 0.004});
        painter.setBrush(colour(style::background));
        painter.drawRoundedRect(QRectF{where.x() - box_width / 2.0, where.y() - height * 0.72,
                                       box_width, height * 1.44},
                                height * 0.18, height * 0.18);
        painter.setBrush(Qt::NoBrush);
        draw_text(painter, where, height, QString::fromStdString(dial.window.text),
                  colour(style::cyan));
    }

    for (const Needle& needle : dial.needles) {
        painter.setPen(QPen{needle_colour(needle.kind), size * needle_width(needle.kind),
                            Qt::SolidLine, Qt::RoundCap});
        painter.drawLine(at(centre, needle.angle_deg + 180.0, radius * needle.tail),
                         at(centre, needle.angle_deg, radius * needle.length));
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(colour(style::primary));
    painter.drawEllipse(QPointF{centre, centre}, size * style::hub_radius,
                        size * style::hub_radius);
    painter.setBrush(Qt::NoBrush);

    paintAnnunciation(painter, size, dial.annunciation, dial.simulated);
    paintTitle(painter, size);
}

void InstrumentItem::paintHorizon(QPainter& painter, double size) {
    const double centre = size / 2.0;
    const double radius = size * style::dial_radius;
    const Horizon horizon = describe_attitude({reading(), secondReading()}, sizing(size));

    if (horizon.has_attitude) {
        painter.save();
        QPainterPath glass;
        glass.addEllipse(QPointF{centre, centre}, radius, radius);
        painter.setClipPath(glass);

        painter.translate(centre, centre);
        painter.rotate(horizon.rotation_deg);
        painter.translate(0.0, radius * horizon.translation);

        const double reach = radius * 3.0;
        painter.fillRect(QRectF{-reach, -reach, reach * 2, reach}, colour(style::sky));
        painter.fillRect(QRectF{-reach, 0, reach * 2, reach}, colour(style::earth));
        painter.setPen(QPen{colour(style::primary), size * style::horizon_line_width});
        painter.drawLine(QPointF{-reach, 0}, QPointF{reach, 0});

        for (const Rung& rung : horizon.ladder) {
            const double y = -radius * rung.offset;
            const double half = radius * rung.half_width;
            painter.setPen(QPen{colour(style::primary), size * style::rung_width});
            painter.drawLine(QPointF{-half, y}, QPointF{half, y});
            if (!rung.text.empty()) {
                const double height = radius * 0.09;
                for (const double side : {-1.0, 1.0}) {
                    draw_text(painter, QPointF{side * (half + height), y}, height,
                              QString::fromStdString(rung.text), colour(style::primary),
                              false);
                }
            }
        }
        painter.restore();
    }

    painter.setPen(QPen{colour(style::bezel), size * 0.010});
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF{centre, centre}, radius, radius);

    for (const Tick& tick : horizon.bank_scale) {
        painter.setPen(QPen{colour(style::primary),
                            size * (tick.kind == TickKind::major ? 0.009 : 0.005)});
        painter.drawLine(at(centre, tick.angle_deg, radius * tick.inner_radius),
                         at(centre, tick.angle_deg, radius * tick.outer_radius));
    }

    if (horizon.has_attitude) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colour(style::amber));
        painter.drawEllipse(at(centre, horizon.bank_pointer_deg, radius * 0.80),
                            size * 0.018, size * 0.018);
        painter.setBrush(Qt::NoBrush);
    }

    // Sagoma dell'aeromobile: fissa, e' il riferimento immobile rispetto a cui
    // il mondo si inclina.
    const double wing = radius * 0.38;
    painter.setPen(QPen{colour(style::amber), size * style::aircraft_width,
                        Qt::SolidLine, Qt::RoundCap});
    painter.drawLine(QPointF{centre - wing, centre}, QPointF{centre - wing * 0.25, centre});
    painter.drawLine(QPointF{centre + wing * 0.25, centre}, QPointF{centre + wing, centre});
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour(style::amber));
    painter.drawEllipse(QPointF{centre, centre}, size * 0.012, size * 0.012);
    painter.setBrush(Qt::NoBrush);

    paintAnnunciation(painter, size, horizon.annunciation, horizon.simulated);
    paintTitle(painter, size);
}

void InstrumentItem::paintTurn(QPainter& painter, double size) {
    const double centre = size / 2.0;
    const double radius = size * style::dial_radius;
    const TurnCoordinator coordinator =
        describe_turn({reading(), secondReading()}, sizing(size));

    painter.setPen(QPen{colour(style::bezel), size * style::bezel_width});
    painter.setBrush(colour(style::face));
    painter.drawEllipse(QPointF{centre, centre}, radius * style::bezel_radius,
                        radius * style::bezel_radius);
    painter.setBrush(Qt::NoBrush);

    for (const Tick& tick : coordinator.standard_marks) {
        painter.setPen(QPen{colour(style::primary), size * style::major_tick_width});
        painter.drawLine(at(centre, tick.angle_deg - 90.0, radius * tick.inner_radius),
                         at(centre, tick.angle_deg - 90.0, radius * tick.outer_radius));
    }

    if (coordinator.has_turn) {
        const double wing = radius * 0.62;
        const double pivot = centre - radius * 0.18;
        painter.save();
        painter.translate(centre, pivot);
        painter.rotate(coordinator.aircraft_bank_deg);
        painter.setPen(QPen{colour(style::primary), size * 0.020, Qt::SolidLine, Qt::RoundCap});
        painter.drawLine(QPointF{-wing, 0}, QPointF{wing, 0});
        painter.setPen(QPen{colour(style::primary), size * 0.016, Qt::SolidLine, Qt::RoundCap});
        painter.drawLine(QPointF{0, -radius * 0.16}, QPointF{0, radius * 0.10});
        painter.restore();
    }

    const double tube_y = centre + radius * 0.55;
    const double tube_half = radius * 0.42;
    painter.setPen(QPen{colour(style::bezel), size * 0.005});
    painter.setBrush(colour(style::background));
    painter.drawRoundedRect(QRectF{centre - tube_half, tube_y - radius * 0.11,
                                   tube_half * 2.0, radius * 0.22},
                            radius * 0.11, radius * 0.11);
    painter.setBrush(Qt::NoBrush);
    for (const double side : {-1.0, 1.0}) {
        const double x = centre + side * radius * 0.11;
        painter.drawLine(QPointF{x, tube_y - radius * 0.11}, QPointF{x, tube_y + radius * 0.11});
    }
    if (coordinator.has_ball) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colour(style::primary));
        painter.drawEllipse(
            QPointF{centre + coordinator.ball_offset * (tube_half - radius * 0.10), tube_y},
            radius * 0.09, radius * 0.09);
        painter.setBrush(Qt::NoBrush);
    }

    paintAnnunciation(painter, size, coordinator.annunciation, coordinator.simulated);
    paintTitle(painter, size);
}

void InstrumentItem::paintAnnunciation(QPainter& painter, double size,
                                       Annunciation annunciation, bool is_simulated) {
    const double centre = size / 2.0;
    const double radius = size * style::dial_radius;

    if (annunciation != Annunciation::none) {
        const QColor text_colour = annunciation == Annunciation::failed
            ? colour(style::warning)
            : colour(style::amber);
        draw_text(painter, QPointF{centre, centre + radius * 0.72},
                  radius * style::annunciation_height,
                  QString::fromLatin1(annunciation_text(annunciation)), text_colour);
    }

    if (is_simulated) {
        const double inset = size * 0.02;
        painter.setPen(QPen{colour(style::amber), size * style::simulation_frame_width});
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF{inset, inset, size - 2 * inset, size - 2 * inset},
                                size * 0.02, size * 0.02);
        draw_text(painter, QPointF{centre, size - inset * 3.0}, radius * 0.12,
                  QStringLiteral("SIMULAZIONE"), colour(style::amber));
    }
}

void InstrumentItem::paintTitle(QPainter& painter, double size) {
    const double centre = size / 2.0;
    const double radius = size * style::dial_radius;
    if (!title_.isEmpty()) {
        const double offset = kind_ == Attitude || kind_ == TurnAndSlip ? 0.62 : 0.38;
        draw_text(painter, QPointF{centre, centre - radius * offset},
                  radius * style::title_height, title_, colour(style::cyan));
    }
    if (!unit_.isEmpty() && kind_ != Attitude && kind_ != TurnAndSlip) {
        draw_text(painter, QPointF{centre, centre + radius * 0.50},
                  radius * style::unit_height, unit_, colour(style::secondary), false);
    }
}

#define SETTER(type, name, member)                    \
    void InstrumentItem::name(type incoming) {        \
        if (member == incoming) return;               \
        member = incoming;                            \
        emit changed();                               \
    }

SETTER(InstrumentItem::Kind, setKind, kind_)
SETTER(double, setValue, value_)
SETTER(int, setValidity, validity_)
SETTER(bool, setSimulated, simulated_)
SETTER(double, setSecondValue, second_value_)
SETTER(int, setSecondValidity, second_validity_)
SETTER(const QString&, setTitle, title_)
SETTER(const QString&, setUnit, unit_)
SETTER(const QString&, setWindowText, window_text_)
SETTER(double, setMinimum, minimum_)
SETTER(double, setMaximum, maximum_)
SETTER(double, setMajorStep, major_step_)
SETTER(double, setMinorStep, minor_step_)
SETTER(const QVariantList&, setBands, bands_)

#undef SETTER
