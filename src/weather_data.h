#pragma once
#include <Arduino.h>

// ============================================================
//  Wetterdaten – per MQTT JSON befüllt
// ============================================================
struct WeatherData {
    float temperature   = NAN;
    float humidity      = NAN;
    float pressure      = NAN;
    float wind_speed    = NAN;
    float wind_bearing  = NAN;
    char  condition[32] = "unknown";
    unsigned long last_update = 0;

    bool isValid() const {
        return !isnan(temperature) && last_update > 0;
    }
};
