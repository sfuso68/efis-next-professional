#pragma once

// Keep the validity gate before reading coordinates. Provenance is specific
// to position, not to unrelated simulated attitude/engine channels.
template <typename Controller, typename Weather>
void updateWeatherPosition(const Controller& controller, Weather& weather) {
    if (!controller.positionValid()) {
        weather.invalidatePosition();
        return;
    }
    weather.updatePosition(controller.latitude(), controller.longitude(),
                           true, controller.positionSimulated());
}
