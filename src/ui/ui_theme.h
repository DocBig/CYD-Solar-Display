#pragma once
#include <lvgl.h>

// ============================================================
//  Farbpalette — moderner Dark-Look, angelehnt ans Original
//  aber etwas wärmer/weicher für Augen-Ergonomie
// ============================================================
namespace theme {
    constexpr uint32_t BG           = 0x0E1116;  // Fast-schwarz, leicht bläulich
    constexpr uint32_t SURFACE      = 0x1A1F26;  // Karten-Hintergrund
    constexpr uint32_t SURFACE_HI   = 0x252B33;  // Hover / Aktiv

    constexpr uint32_t TEXT         = 0xF5F7FA;  // Haupttext (heller, knackiger)
    constexpr uint32_t TEXT_DIM     = 0xA8B0BA;  // Sekundärtext
    constexpr uint32_t TEXT_MUTED   = 0x6C7380;  // Labels / Hinweise

    // Kräftigere, sattere Akzentfarben
    constexpr uint32_t ACCENT       = 0x00D4FF;  // Cyan-Blau (Uhrzeit)
    constexpr uint32_t GOOD         = 0x00E676;  // Kräftiges Grün
    constexpr uint32_t MID          = 0xFF9500;  // Sattes Orange
    constexpr uint32_t BAD          = 0xFF3B30;  // Sattes Rot
    constexpr uint32_t PV           = 0xFFD60A;  // Knalliges Gelb-Gold

    constexpr uint32_t ARC_BG       = 0x2A2F38;  // Gauge-Hintergrund-Bogen
}

// ============================================================
//  Batterie-Schwellwerte → Farbe
// ============================================================
inline lv_color_t soc_color(float soc) {
    if (soc < 20.0f) return lv_color_hex(theme::BAD);
    if (soc < 50.0f) return lv_color_hex(theme::MID);
    return lv_color_hex(theme::GOOD);
}

// ============================================================
//  Globalen Dunkel-Hintergrund auf Active-Screen anwenden
// ============================================================
inline void theme_apply_root(lv_obj_t* scr) {
    lv_obj_set_style_bg_color(scr, lv_color_hex(theme::BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr,   LV_OPA_COVER,             LV_PART_MAIN);
    lv_obj_set_style_text_color(scr, lv_color_hex(theme::TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(scr, &lv_font_montserrat_14,  LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}
