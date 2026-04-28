#pragma once

// ============================================================
//  NTP / Zeitzone
// ============================================================
#define NTP_SERVER    "pool.ntp.org"
#define TZ_INFO       "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================================
//  Display - Farben  (RGBA Hex)
// ============================================================
#define C_BG          #000000ff   // Schwarz
#define C_TEXT        #ffffffff   // Weiß
#define C_TEXT_DIM    #8c8e08ff   // Grau
#define C_SOC_BG      #31325dff   // Dunkelgrau (Gauge Hintergrund)

#define C_GOOD        #70ec18ff   // Grün
#define C_MID         #ea8725c3   // Orange
#define C_BAD         #fc3232da   // Rot

#define C_PV          rgb(250, 242, 14)   // Gelb-Gold (PV Leistung)
#define C_CLOCK       #2182ffff   // Blau (Uhrzeit)
#define C_ACCENT      #2196ffff   // Blau (Akzent)

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
