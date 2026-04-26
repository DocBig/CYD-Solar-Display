#pragma once
#include <Arduino.h>

// ============================================================
//  Solardaten – werden per MQTT befüllt
//
//  Felder sind in zwei Gruppen geteilt:
//    1. AKTIV verwendet — werden im Display dargestellt
//    2. RESERVIERT     — werden empfangen, aber (noch) nicht
//                        angezeigt. Fuer spaetere Erweiterungen
//                        belassen, koennen aber bei Bedarf
//                        aus dem TOPIC_MAPS-Array unten entfernt
//                        werden um MQTT-Subscriptions zu sparen.
// ============================================================
struct SolarData {
    // ── 1. AKTIV verwendet ──────────────────────────────────────
    // PV-Leistung
    float pv_power      = NAN;   // PV-Leistung gesamt → grosse Zahl
    float pv1_power     = NAN;   // PV-String 1        → "Sued:"
    float pv2_power     = NAN;   // PV-String 2        → "West:"

    // Batterie
    float bat_soc       = NAN;   // SOC %              → innerer Ring + Zahl
    float bat_current   = NAN;   // Lade-/Entladestrom → rechts mit Pfeil

    // Netz / Last
    float grid_power    = NAN;   // grid_ct_power      → Footer mit Richtung
    float load_power    = NAN;   // Hausverbrauch      → Footer mit Haus-Icon

    // Tageswerte
    float day_pv        = NAN;   // PV-Tagesertrag     → Footer + aeusserer Ring
    float day_import    = NAN;   // Tages-Netzbezug    → Footer rot

    // ── 2. RESERVIERT (empfangen, nicht angezeigt) ──────────────
    // Diese Werte landen aktuell zwar in der Struct, werden aber
    // nirgends im UI dargestellt. Belassen fuer spaetere Features
    // wie z.B. eine Tagesstatistik-Seite oder Netz-Offline-Warnung.
    float bat_power     = NAN;   // Batterieleistung in W
    float bat_voltage   = NAN;   // Batteriespannung
    float day_export    = NAN;   // Tages-Einspeisung
    float day_load      = NAN;   // Tages-Hausverbrauch
    float day_bat_chg   = NAN;   // Tages-Akku-Ladung
    float day_bat_dis   = NAN;   // Tages-Akku-Entladung

    // Status
    bool  grid_connected = true; // empfangen, aktuell nicht ausgewertet
    unsigned long last_update = 0;
};

// ============================================================
//  MQTT Topic → Struct-Feld Mapping
//
//  Die Topics werden alle subscribed und befuellt. Wer Bandbreite
//  / RAM sparen will, kann die mit "RESERVIERT" markierten Zeilen
//  auskommentieren — die zugehoerigen Felder bleiben dann auf NaN
//  bzw. ihrem Default-Wert.
// ============================================================
struct TopicMap {
    const char* suffix;
    float SolarData::* field;
};

const TopicMap TOPIC_MAPS[] = {
    // AKTIV verwendet
    { "pv_power",              &SolarData::pv_power },
    { "pv1_power",             &SolarData::pv1_power },
    { "pv2_power",             &SolarData::pv2_power },
    { "battery_soc",           &SolarData::bat_soc },
    { "battery_current",       &SolarData::bat_current },
    { "grid_ct_power",         &SolarData::grid_power },
    { "load_power",            &SolarData::load_power },
    { "day_pv_energy",         &SolarData::day_pv },
    { "day_grid_import",       &SolarData::day_import },

    // RESERVIERT - empfangen, aber (noch) nicht angezeigt
 //   { "battery_power",         &SolarData::bat_power },
 //   { "battery_voltage",       &SolarData::bat_voltage },
 //   { "day_grid_export",       &SolarData::day_export },
 //   { "day_load_energy",       &SolarData::day_load },
 //   { "day_battery_charge",    &SolarData::day_bat_chg },
 //   { "day_battery_discharge", &SolarData::day_bat_dis },
};

const int TOPIC_COUNT = sizeof(TOPIC_MAPS) / sizeof(TOPIC_MAPS[0]);