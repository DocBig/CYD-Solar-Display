#pragma once

// ============================================================
//  NTP / Zeitzone
// ============================================================
#define NTP_SERVER    "pool.ntp.org"
#define TZ_INFO       "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================================
//  Display - Farben  (RGB565)
// ============================================================
#define C_BG          0x0000   // Schwarz
#define C_TEXT        0xFFFF   // Weiß
#define C_TEXT_DIM    0x8C71   // Grau
#define C_SOC_BG      0x632C   // Dunkelgrau (Gauge Hintergrund)

#define C_GOOD        #70ec18   // Grün
#define C_MID         #ed8116b5   // Orange
#define C_BAD         #fc3232da   // Rot

#define C_PV          0xFE80   // Gelb-Gold (PV Leistung)
#define C_CLOCK       0x041F   // Blau (Uhrzeit)
#define C_ACCENT      0x04BF   // Blau (Akzent)

// ============================================================
//  Batterie-Schwellwerte
// ============================================================
#define BAT_LOW_PCT   50
#define BAT_CRIT_PCT  20

// ============================================================
//  Gauge Geometrie
// ============================================================
#define GAUGE_CX      120
#define GAUGE_CY      150
#define GAUGE_RADIUS  105
#define GAUGE_ANGLE_START  -100
#define GAUGE_ANGLE_END     100
#define GAUGE_THICK_BG  20
#define GAUGE_THICK_FG  12

// ============================================================
//  Animierte Punkte
// ============================================================
#define DOT_SPEED     0.020f
#define DOTS_PER_RING 10
#define DOT_SPACING   (0.14f / DOTS_PER_RING)
#define DOT_R_OUTER   (GAUGE_RADIUS + 15)
#define DOT_R_INNER   (GAUGE_RADIUS + 8)

// ============================================================
//  AP-Modus
// ============================================================
#define AP_NAME       "SolarDisplay-Setup"
#define AP_TRIGGER_PIN  0   // BOOT-Taste: 3s gedrückt → AP-Modus

// ============================================================
//  Wetter MQTT
// ============================================================
#define WEATHER_TOPIC "wetter/koewa"
