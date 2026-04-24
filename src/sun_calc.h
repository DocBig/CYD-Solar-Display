#pragma once
#include <Arduino.h>
#include <math.h>
#include <time.h>

// ============================================================
//  Vereinfachte Sonnenstandsberechnung
//  Braucht nur: Breitengrad, Längengrad, Datum (via NTP)
//  Genauigkeit: ~2 Minuten
// ============================================================

class SunCalc {
public:
    float latitude  = 51.18f;   // Default: Bautzen
    float longitude = 14.42f;

    // Sonnenaufgang/-untergang in Stunden (Lokalzeit)
    float sunrise = 6.0f;
    float sunset  = 20.0f;

    // Ist es gerade Nacht?
    bool isNight() {
        struct tm ti;
        if (!getLocalTime(&ti, 100)) return false;

        float now = ti.tm_hour + ti.tm_min / 60.0f;
        return (now < sunrise || now >= sunset);
    }

    // Neu berechnen (einmal pro Stunde reicht)
    void update() {
        struct tm ti;
        if (!getLocalTime(&ti, 100)) return;

        // Tag des Jahres (1-366)
        int N = ti.tm_yday + 1;

        // Deklination der Sonne (vereinfacht)
        float decl = -23.45f * cosf(2.0f * PI * (N + 10) / 365.0f);
        float declRad = decl * DEG_TO_RAD;
        float latRad  = latitude * DEG_TO_RAD;

        // Stundenwinkel bei Sonnenauf-/untergang
        float cosH = -tanf(latRad) * tanf(declRad);

        // Polarregion-Check
        if (cosH < -1.0f) {
            // Mitternachtssonne
            sunrise = 0.0f;
            sunset  = 24.0f;
            return;
        }
        if (cosH > 1.0f) {
            // Polarnacht
            sunrise = 12.0f;
            sunset  = 12.0f;
            return;
        }

        float H = acosf(cosH) * RAD_TO_DEG;

        // Zeitgleichung (Equation of Time, vereinfacht)
        float B = 2.0f * PI * (N - 81) / 365.0f;
        float EoT = 9.87f * sinf(2 * B) - 7.53f * cosf(B) - 1.5f * sinf(B);

        // Sonnen-Mittag in UTC (Stunden)
        float solarNoonUTC = 12.0f - longitude / 15.0f - EoT / 60.0f;

        // Zeitzone berechnen (Differenz Lokalzeit - UTC)
        time_t now = time(nullptr);
        struct tm utc;
        gmtime_r(&now, &utc);
        float tzOffset = (ti.tm_hour - utc.tm_hour) + (ti.tm_min - utc.tm_min) / 60.0f;
        // Tageswechsel korrigieren
        if (tzOffset > 12) tzOffset -= 24;
        if (tzOffset < -12) tzOffset += 24;

        // Sonnenauf-/untergang in Lokalzeit
        sunrise = solarNoonUTC - H / 15.0f + tzOffset;
        sunset  = solarNoonUTC + H / 15.0f + tzOffset;

        // Clamp
        if (sunrise < 0) sunrise += 24.0f;
        if (sunset > 24) sunset -= 24.0f;
    }
};