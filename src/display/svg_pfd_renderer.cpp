#include "efis/display/svg_pfd_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>

namespace efis::display {
namespace {

std::string number(double value, int precision = 0) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::optional<double> usable_value(const DisplayField& field) {
    if (!is_usable(field.validity) || !std::isfinite(field.value)) return std::nullopt;
    return field.value;
}

std::string displayed_number(const DisplayField& field, int precision = 0) {
    const auto value = usable_value(field);
    return value ? number(*value, precision) : "---";
}

const char* field_color(const DisplayField& field) {
    if (!is_usable(field.validity)) return "#FF3B30";
    if (field.validity == Validity::degraded) return "#FFB000";
    return "#F4F7FA";
}

const char* profile_name(DisplayProfile profile) {
    switch (profile) {
        case DisplayProfile::standard: return "STD";
        case DisplayProfile::professional: return "PRO";
        case DisplayProfile::degraded: return "DEG";
    }
    return "STD";
}

}  // namespace

std::string SvgPfdRenderer::render(const PfdView& v, RenderSize size) const {
    const int w = std::max(size.width, 640);
    const int h = std::max(size.height, 360);
    const bool compact = w < 1000 || h < 600;
    const double top = compact ? 52.0 : 70.0;
    const double bottom = compact ? 48.0 : 70.0;
    const double tape_w = compact ? w * 0.14 : w * 0.13;
    const double horizon_x = tape_w + 18.0;
    const double horizon_w = w - 2.0 * horizon_x;
    const double horizon_h = h - top - bottom;
    const double cx = w / 2.0;
    const double cy = top + horizon_h / 2.0;
    const auto roll = usable_value(v.roll_deg);
    const auto pitch = usable_value(v.pitch_deg);
    const bool attitude_usable = roll.has_value() && pitch.has_value();
    const double pitch_offset = attitude_usable ? *pitch * horizon_h / 40.0 : 0.0;
    const double hsi_r = compact ? h * 0.12 : h * 0.14;
    const double hsi_y = h - bottom - hsi_r - 8.0;
    const double ui_scale = std::clamp(std::min(w / 1024.0, h / 600.0), 0.8, 1.8);

    std::ostringstream s;
    s << "<svg xmlns='http://www.w3.org/2000/svg' width='" << w
      << "' height='" << h << "' viewBox='0 0 " << w << ' ' << h << "'>"
      << "<rect width='100%' height='100%' fill='#071019'/>"
      << "<style>text{font-family:'Arial Narrow','Segoe UI',sans-serif;fill:#F4F7FA}"
         ".label{fill:#9AAEBD;font-weight:600}.cyan{fill:#00C8F0}.green{fill:#58D330}"
         ".line{stroke:#F4F7FA;stroke-width:2;fill:none}</style>"
      << "<text x='24' y='34' font-size='24' font-weight='700'>EFIS <tspan class='cyan'>NEXT</tspan></text>"
      << "<text x='" << cx << "' y='32' text-anchor='middle' font-size='18' class='label'>PFD • "
      << profile_name(v.profile) << " • SEQ " << v.sequence << "</text>"
      << "<defs><clipPath id='att'><rect x='" << horizon_x << "' y='" << top
      << "' width='" << horizon_w << "' height='" << horizon_h << "' rx='5'/></clipPath></defs>";

    if (attitude_usable) {
        s << "<g id='attitude-valid' clip-path='url(#att)'>"
          << "<g transform='rotate(" << -*roll << ' ' << cx << ' ' << cy << ")'>"
          << "<rect x='" << (horizon_x - h) << "' y='" << (top - h + pitch_offset)
          << "' width='" << (horizon_w + 2*h) << "' height='" << (horizon_h/2 + h)
          << "' fill='#087AC1'/><rect x='" << (horizon_x - h) << "' y='" << (cy + pitch_offset)
          << "' width='" << (horizon_w + 2*h) << "' height='" << (horizon_h/2 + h)
          << "' fill='#795126'/><line x1='" << (horizon_x-h) << "' y1='" << (cy+pitch_offset)
          << "' x2='" << (horizon_x+horizon_w+h) << "' y2='" << (cy+pitch_offset)
          << "' stroke='#F4F7FA' stroke-width='3'/>";

        for (int ladder_pitch = -20; ladder_pitch <= 20; ladder_pitch += 5) {
            if (ladder_pitch == 0) continue;
            const double y = cy + pitch_offset - ladder_pitch * horizon_h / 40.0;
            const double half = (ladder_pitch % 10 == 0) ? 48.0 : 26.0;
            s << "<line x1='" << (cx-half) << "' y1='" << y << "' x2='" << (cx+half)
              << "' y2='" << y << "' stroke='#F4F7FA' stroke-width='2'/>"
              << "<text x='" << (cx-half-8) << "' y='" << (y+5)
              << "' text-anchor='end' font-size='14'>" << std::abs(ladder_pitch) << "</text>";
        }
        s << "</g></g>";
    } else {
        s << "<g id='attitude-invalid' clip-path='url(#att)'>"
          << "<rect x='" << horizon_x << "' y='" << top << "' width='" << horizon_w
          << "' height='" << horizon_h << "' fill='#101820'/>"
          << "<path d='M " << horizon_x << ' ' << top << " l " << horizon_w << ' ' << horizon_h
          << " M " << (horizon_x+horizon_w) << ' ' << top << " l " << -horizon_w << ' ' << horizon_h
          << "' stroke='#61252A' stroke-width='3'/>"
          << "<rect x='" << (cx-122) << "' y='" << (cy-30) << "' width='244' height='60' rx='5'"
          << " fill='#2A1014' stroke='#FF3B30' stroke-width='3'/>"
          << "<text x='" << cx << "' y='" << (cy+8)
          << "' text-anchor='middle' font-size='22' fill='#FF3B30' font-weight='800'>ATTITUDE INVALID</text></g>";
    }
    s << "<rect x='" << horizon_x << "' y='" << top << "' width='" << horizon_w
      << "' height='" << horizon_h << "' rx='5' fill='none' stroke='#263B4A' stroke-width='2'/>"
      << "<path d='M " << (cx-72) << ' ' << cy << " h48 l24 16 24-16 h48' stroke='#FFB000' stroke-width='5' fill='none'/>"
      << "<circle cx='" << cx << "' cy='" << (cy-22) << "' r='8' fill='none' stroke='#00C8F0' stroke-width='3'/>"
      << "<line x1='" << cx << "' y1='" << (cy-14) << "' x2='" << cx << "' y2='" << (cy+10) << "' stroke='#00C8F0' stroke-width='3'/>";

    const auto commanded_roll = usable_value(v.commanded_roll_deg);
    const auto commanded_pitch = usable_value(v.commanded_pitch_deg);
    if (v.flight_director_active && attitude_usable && commanded_roll && commanded_pitch) {
        const double fd_x = cx + (*commanded_roll - *roll) * 4.0 * ui_scale;
        const double fd_y = cy - (*commanded_pitch - *pitch) * 5.0 * ui_scale;
        s << "<path d='M " << (fd_x-62*ui_scale) << ' ' << fd_y << " h42 l20 -14 l20 14 h42'"
          << " stroke='#D829FF' stroke-width='" << (5*ui_scale) << "' fill='none'/>";
    }

    // Bank-angle scale is fixed to the aircraft symbol, not the moving horizon.
    s << "<path d='M " << (cx-110*ui_scale) << ' ' << (top+42*ui_scale)
      << " Q " << cx << ' ' << (top-12*ui_scale) << ' ' << (cx+110*ui_scale) << ' '
      << (top+42*ui_scale) << "' stroke='#F4F7FA' stroke-width='2' fill='none'/>"
      << "<path d='M " << cx << ' ' << (top+3*ui_scale) << " l-9 16 h18 z' fill='#F4F7FA'/>";

    auto tape = [&](double x, const char* title, const DisplayField& f, const char* unit) {
        const double y = top + 16;
        const double th = horizon_h - 32;
        s << "<rect x='" << x << "' y='" << y << "' width='" << tape_w << "' height='" << th
          << "' rx='5' fill='#071019' fill-opacity='.84' stroke='#547080'/>"
          << "<text x='" << (x+tape_w/2) << "' y='" << (y+24) << "' text-anchor='middle' font-size='14' class='label'>"
          << title << " • " << unit << "</text>"
          << "<rect x='" << (x+5) << "' y='" << (cy-30) << "' width='" << (tape_w-10)
          << "' height='60' fill='#0B1722' stroke='" << field_color(f) << "' stroke-width='2'/>"
          << "<text x='" << (x+tape_w/2) << "' y='" << (cy+13) << "' text-anchor='middle' font-size='"
          << (compact ? 30 : 38) << "' font-weight='800' fill='" << field_color(f) << "'>"
          << displayed_number(f) << "</text>";
        if (const char* flag = field_label(f); flag[0] != '\0')
            s << "<text x='" << (x+tape_w/2) << "' y='" << (y+th-12)
              << "' text-anchor='middle' font-size='14' fill='" << field_color(f) << "'>" << flag << "</text>";
    };
    tape(14, "IAS", v.airspeed_kt, "KT");
    tape(w-tape_w-14, "ALT", v.altitude_ft, "FT");

    // Render only selections actually present in the view model. No inferred
    // autopilot mode or trend is permitted in the reference renderer.
    s << "<text id='selected-altitude-value' x='" << (w-tape_w/2-14) << "' y='" << (top+58)
      << "' text-anchor='middle' font-size='14' fill='" << field_color(v.selected_altitude_ft)
      << "'>SEL " << displayed_number(v.selected_altitude_ft) << "</text>";

    if (v.flight_director_active && usable_value(v.selected_altitude_ft)) {
        s << "<text id='flight-director-mode' x='" << (w - 24)
          << "' y='32' text-anchor='end' font-size='18' class='green'>FD • ALT SEL "
          << displayed_number(v.selected_altitude_ft) << "</text>";
    }

    s << "<g><circle cx='" << cx << "' cy='" << hsi_y << "' r='" << hsi_r
      << "' fill='#071019' fill-opacity='.9' stroke='#9AAEBD' stroke-width='2'/>"
      << "<text x='" << cx << "' y='" << (hsi_y-hsi_r+22) << "' text-anchor='middle' font-size='14' class='label'>HDG</text>"
      << "<text id='heading-value' x='" << cx << "' y='" << (hsi_y+12) << "' text-anchor='middle' font-size='"
      << (compact ? 28 : 38) << "' font-weight='800' fill='" << field_color(v.heading_deg) << "'>"
      << displayed_number(v.heading_deg) << (usable_value(v.heading_deg) ? "°" : "") << "</text>"
      << "<path d='M " << cx << ' ' << (hsi_y-hsi_r+30) << " l-10 18 h20 z' fill='#00C8F0'/></g>"
      << "<text x='24' y='" << (h-18) << "' font-size='16' class='label'>GS "
      << "<tspan id='ground-speed-value' fill='" << field_color(v.ground_speed_kt) << "'>"
      << displayed_number(v.ground_speed_kt) << (usable_value(v.ground_speed_kt) ? " KT" : "") << "</tspan></text>"
      << "<text x='" << (w-24) << "' y='" << (h-18) << "' text-anchor='end' font-size='16' class='label'>V/S "
      << "<tspan id='vertical-speed-value' fill='" << field_color(v.vertical_speed_fpm) << "'>"
      << displayed_number(v.vertical_speed_fpm) << (usable_value(v.vertical_speed_fpm) ? " FPM" : "") << "</tspan></text>"
      << "<text id='selected-course-value' x='" << cx << "' y='" << (h-18)
      << "' text-anchor='middle' font-size='15' fill='" << field_color(v.selected_course_deg) << "'>CRS "
      << displayed_number(v.selected_course_deg) << (usable_value(v.selected_course_deg) ? "°" : "") << "</text>";

    if (!v.alerts.empty()) {
        const auto& alert = v.alerts.front();
        const char* color = alert.severity == AlertSeverity::warning ? "#FF3B30" :
                            alert.severity == AlertSeverity::caution ? "#FFB000" : "#00C8F0";
        s << "<rect x='" << (cx-110) << "' y='40' width='220' height='25' rx='3' fill='" << color
          << "' fill-opacity='.2' stroke='" << color << "'/>"
          << "<text x='" << cx << "' y='58' text-anchor='middle' font-size='14' fill='" << color
          << "' font-weight='700'>" << alert.message << "</text>";
        if (v.alerts.size() > 1) {
            s << "<text id='additional-alert-count' x='" << (cx+102) << "' y='58' text-anchor='end'"
              << " font-size='11' fill='" << color << "' font-weight='700'>+"
              << (v.alerts.size()-1) << " MORE</text>";
        }
    }
    s << "</svg>";
    return s.str();
}

}  // namespace efis::display
