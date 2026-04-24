#pragma once
#include <Arduino.h>

// ============================================================
//  Solardaten – werden per MQTT befüllt
// ============================================================
struct SolarData {
    // PV
    float pv_power      = NAN;
    float pv1_power     = NAN;
    float pv2_power     = NAN;

    // Batterie
    float bat_soc       = NAN;
    float bat_power     = NAN;
    float bat_current   = NAN;
    float bat_voltage   = NAN;

    // Grid
    float grid_power    = NAN;   // grid_ct_power

    // Load
    float load_power    = NAN;

    // Tageswerte
    float day_pv        = NAN;
    float day_export    = NAN;
    float day_import    = NAN;
    float day_load      = NAN;
    float day_bat_chg   = NAN;
    float day_bat_dis   = NAN;

    // Status
    bool  grid_connected = true;
    unsigned long last_update = 0;
};

// ============================================================
//  MQTT Topic → Struct-Feld Mapping
// ============================================================
struct TopicMap {
    const char* suffix;
    float SolarData::* field;
};

const TopicMap TOPIC_MAPS[] = {
    { "pv_power",              &SolarData::pv_power },
    { "pv1_power",             &SolarData::pv1_power },
    { "pv2_power",             &SolarData::pv2_power },
    { "battery_soc",           &SolarData::bat_soc },
    { "battery_power",         &SolarData::bat_power },
    { "battery_current",       &SolarData::bat_current },
    { "battery_voltage",       &SolarData::bat_voltage },
    { "grid_ct_power",         &SolarData::grid_power },
    { "load_power",            &SolarData::load_power },
    { "day_pv_energy",         &SolarData::day_pv },
    { "day_grid_export",       &SolarData::day_export },
    { "day_grid_import",       &SolarData::day_import },
    { "day_load_energy",       &SolarData::day_load },
    { "day_battery_charge",    &SolarData::day_bat_chg },
    { "day_battery_discharge", &SolarData::day_bat_dis },
};

const int TOPIC_COUNT = sizeof(TOPIC_MAPS) / sizeof(TOPIC_MAPS[0]);
