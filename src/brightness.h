#pragma once
// ============================================================
//  Brightness-Controller
//
//  Berechnet aus Sonnenauf-/untergangszeit die aktuelle
//  Display-Helligkeit mit sanfter Daemmerungsblende (±1h).
//
//  Kurve (Stundenwert der Lokalzeit):
//
//    bright_max  ─────────────╱───────╲──────
//                          ╱           ╲
//    bright_min  ──────╱                 ╲──
//                 SR-1h   SR        SS   SS+1h
//
//  Uebergangsphase: Cosine-Ease (weicher als linear).
// ============================================================
#include <Arduino.h>
#include <math.h>
#include <time.h>

#include "sun_calc.h"

class Brightness {
public:
    // Wie lang soll die Daemmerungsphase sein? (in Stunden)
    float fade_hours = 1.0f;

    // Rueckgabe: 0..255 PWM-Wert fuers Backlight
    // Bei fehlender Uhrzeit: liefere max zurueck (Fallback).
    uint8_t compute(const SunCalc& sun, uint8_t bright_min, uint8_t bright_max) const {
        struct tm ti;
        if (!getLocalTime(&ti, 10)) {
            return bright_max;   // Noch keine Uhrzeit → hell lassen
        }
        float now = ti.tm_hour + ti.tm_min / 60.0f;

        float sr = sun.sunrise;
        float ss = sun.sunset;

        // Sonderfaelle Polar
        if (sr <= 0.0f && ss >= 24.0f) return bright_max;   // Mitternachtssonne
        if (sr >= ss)                  return bright_min;   // Polarnacht

        // Dämmerungsfenster um sunrise/sunset
        float sr_start = sr - fade_hours;   // Start des Aufblendens
        float ss_end   = ss + fade_hours;   // Ende des Abblendens

        // Heller Tag (ohne Daemmerung)
        if (now >= sr && now <= ss) {
            return bright_max;
        }

        // Morgenblende: sr_start <= now < sr → von min nach max
        if (now >= sr_start && now < sr) {
            float t = (now - sr_start) / fade_hours;   // 0..1
            return interp(bright_min, bright_max, t);
        }

        // Abendblende: ss < now <= ss_end → von max nach min
        if (now > ss && now <= ss_end) {
            float t = (now - ss) / fade_hours;         // 0..1
            return interp(bright_max, bright_min, t);
        }

        // Sonst: Nacht
        return bright_min;
    }

private:
    // Cosine-Ease: sanfter Uebergang, wie "ease in out"
    static uint8_t interp(uint8_t from, uint8_t to, float t) {
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        float eased = 0.5f * (1.0f - cosf(t * PI));   // 0..1, weich
        float v = (float)from + ((float)to - (float)from) * eased;
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
        return (uint8_t)(v + 0.5f);
    }
};