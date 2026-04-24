#pragma once
// ============================================================
//  Solar-Seite - LVGL Version 4
//
//  Grosser zentraler Arc, keine BATTERIE-Beschriftung,
//  Batteriestrom rechts neben der PV-Leistung.
//
//  Layout (240x320):
//  - Header        y=  0..22   Clock + WiFi
//  - Arc (180x180) y= 25..205  dominanter SOC-Ring, zentriert
//  - PV-Leistung   y=210..255  (links, 40pt) + Einheit + Strom (rechts, klein)
//  - Sued/West     y=260       zwei Spalten, 14pt
//  - Netz/Last     y=282       zwei Spalten, 16pt mit Icons
//  - Tag/Export    y=302       zwei Spalten, 16pt mit Icons
// ============================================================
#include <lvgl.h>
#include <Arduino.h>
#include <time.h>

#include "ui_theme.h"
#include "solar_data.h"

class UiSolar {
public:
    void create(lv_obj_t* parent) {
        theme_apply_root(parent);
        _root = parent;

        build_header();
        build_soc_block();
        build_power_block();
        build_footer();
    }

    // Setzt die Beschriftungen fuer PV1/PV2 (z.B. "Sued:" / "West:").
    // Kann jederzeit aufgerufen werden; wirkt beim naechsten update().
    void set_pv_labels(const char* pv1, const char* pv2) {
        if (pv1) { strncpy(_pv1_label, pv1, sizeof(_pv1_label) - 1);
                   _pv1_label[sizeof(_pv1_label) - 1] = '\0'; }
        if (pv2) { strncpy(_pv2_label, pv2, sizeof(_pv2_label) - 1);
                   _pv2_label[sizeof(_pv2_label) - 1] = '\0'; }
    }

    void update(const SolarData& d) {
        char buf[32];

        // --- SOC ---
        float soc = isnanf(d.bat_soc) ? 0.f : d.bat_soc;
        if (soc < 0) soc = 0; if (soc > 100) soc = 100;

        lv_arc_set_value(_arc, (int32_t)soc);
        lv_obj_set_style_arc_color(_arc, soc_color(soc), LV_PART_INDICATOR);

        snprintf(buf, sizeof(buf), "%d", (int)soc);
        lv_label_set_text(_lbl_soc, buf);
        lv_obj_set_style_text_color(_lbl_soc, soc_color(soc), 0);

        // --- Batteriestrom rechts neben Leistung ---
        if (isnanf(d.bat_current)) {
            lv_label_set_text(_lbl_current, "-- A");
            lv_obj_set_style_text_color(_lbl_current, lv_color_hex(theme::TEXT_MUTED), 0);
        } else {
            float a = d.bat_current;
            // Laden (positiv): Pfeil nach unten, gelb (Strom fliesst IN den Akku)
            // Entladen (negativ): Pfeil nach oben, gruen (Strom fliesst AUS dem Akku)
            if (a > 0.05f)       snprintf(buf, sizeof(buf), LV_SYMBOL_DOWN " %.1f A", a);
            else if (a < -0.05f) snprintf(buf, sizeof(buf), LV_SYMBOL_UP   " %.1f A", -a);
            else                 snprintf(buf, sizeof(buf), "0.0 A");
            lv_label_set_text(_lbl_current, buf);
            lv_obj_set_style_text_color(_lbl_current,
                a >  0.05f ? lv_color_hex(theme::PV)   :
                a < -0.05f ? lv_color_hex(theme::GOOD) :
                lv_color_hex(theme::TEXT_MUTED), 0);
        }

        // --- PV Gesamtleistung ---
        float pv = isnanf(d.pv_power) ? 0.f : d.pv_power;
        if (pv < 1000) snprintf(buf, sizeof(buf), "%d", (int)pv);
        else           snprintf(buf, sizeof(buf), "%.2f", pv / 1000.0f);
        lv_label_set_text(_lbl_power, buf);
        lv_label_set_text(_lbl_power_unit, pv < 1000 ? "W" : "kW");

        // Einheit an Zahl heranschieben (nach Breitenberechnung)
        lv_obj_update_layout(_lbl_power);
        lv_obj_align_to(_lbl_power_unit, _lbl_power,
                        LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -4);

        // --- PV1 + PV2 mit konfigurierbaren Labels ---
        if (isnanf(d.pv1_power)) {
            snprintf(buf, sizeof(buf), "%s --", _pv1_label);
            lv_label_set_text(_lbl_sued, buf);
        } else {
            snprintf(buf, sizeof(buf), "%s %d W", _pv1_label, (int)d.pv1_power);
            lv_label_set_text(_lbl_sued, buf);
        }

        if (isnanf(d.pv2_power)) {
            snprintf(buf, sizeof(buf), "%s --", _pv2_label);
            lv_label_set_text(_lbl_west, buf);
        } else {
            snprintf(buf, sizeof(buf), "%s %d W", _pv2_label, (int)d.pv2_power);
            lv_label_set_text(_lbl_west, buf);
        }

        // --- Netz mit Richtung ---
        float grid = isnanf(d.grid_power) ? 0.f : d.grid_power;
        if (grid > 5.0f) {
            snprintf(buf, sizeof(buf), LV_SYMBOL_DOWN "  %d W", (int)grid);
            lv_obj_set_style_text_color(_lbl_grid, lv_color_hex(theme::BAD), 0);
        } else if (grid < -5.0f) {
            snprintf(buf, sizeof(buf), LV_SYMBOL_UP "  %d W", (int)-grid);
            lv_obj_set_style_text_color(_lbl_grid, lv_color_hex(theme::GOOD), 0);
        } else {
            snprintf(buf, sizeof(buf), LV_SYMBOL_REFRESH "  0 W");
            lv_obj_set_style_text_color(_lbl_grid, lv_color_hex(theme::TEXT_MUTED), 0);
        }
        lv_label_set_text(_lbl_grid, buf);

        // --- Hausverbrauch ---
        float load = isnanf(d.load_power) ? 0.f : d.load_power;
        if (load < 1000) snprintf(buf, sizeof(buf), LV_SYMBOL_HOME "  %d W", (int)load);
        else             snprintf(buf, sizeof(buf), LV_SYMBOL_HOME "  %.2f kW", load / 1000.0f);
        lv_label_set_text(_lbl_load, buf);

        // --- Tagesertrag PV ---
        float day_pv = isnanf(d.day_pv) ? 0.f : d.day_pv;
        snprintf(buf, sizeof(buf), LV_SYMBOL_CHARGE "  %.1f kWh", day_pv);
        lv_label_set_text(_lbl_day, buf);

        // --- Tages-Netzbezug (Import) ---
        float day_imp = isnanf(d.day_import) ? 0.f : d.day_import;
        snprintf(buf, sizeof(buf), LV_SYMBOL_DOWNLOAD "  %.1f kWh", day_imp);
        lv_label_set_text(_lbl_export, buf);
    }

    void update_clock() {
        struct tm t;
        if (getLocalTime(&t, 10)) {
            char buf[12];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
            lv_label_set_text(_lbl_clock, buf);
        }
    }

    void update_wifi(int rssi, bool online) {
        const char* sym;
        lv_color_t  col;
        if (!online)         { sym = LV_SYMBOL_CLOSE; col = lv_color_hex(theme::BAD); }
        else if (rssi > -60) { sym = LV_SYMBOL_WIFI;  col = lv_color_hex(theme::GOOD); }
        else if (rssi > -75) { sym = LV_SYMBOL_WIFI;  col = lv_color_hex(theme::MID); }
        else                 { sym = LV_SYMBOL_WIFI;  col = lv_color_hex(theme::BAD); }
        lv_label_set_text(_lbl_wifi, sym);
        lv_obj_set_style_text_color(_lbl_wifi, col, 0);
    }

private:
    lv_obj_t* _root        = nullptr;
    lv_obj_t* _lbl_clock   = nullptr;
    lv_obj_t* _lbl_wifi    = nullptr;
    lv_obj_t* _arc         = nullptr;
    lv_obj_t* _lbl_soc     = nullptr;
    lv_obj_t* _lbl_current = nullptr;
    lv_obj_t* _lbl_power   = nullptr;
    lv_obj_t* _lbl_power_unit = nullptr;
    lv_obj_t* _lbl_sued    = nullptr;
    lv_obj_t* _lbl_west    = nullptr;

    // PV-Labels — per set_pv_labels() aus Settings befuellt
    char _pv1_label[16] = "PV1:";
    char _pv2_label[16] = "PV2:";
    lv_obj_t* _lbl_grid    = nullptr;
    lv_obj_t* _lbl_load    = nullptr;
    lv_obj_t* _lbl_day     = nullptr;
    lv_obj_t* _lbl_export  = nullptr;

    // ============================================================
    //  Header
    // ============================================================
    void build_header() {
        _lbl_clock = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_clock, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_lbl_clock, lv_color_hex(theme::ACCENT), 0);
        lv_label_set_text(_lbl_clock, "--:--:--");
        lv_obj_align(_lbl_clock, LV_ALIGN_TOP_LEFT, 8, 6);

        _lbl_wifi = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_wifi, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_lbl_wifi, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(_lbl_wifi, LV_SYMBOL_WIFI);
        lv_obj_align(_lbl_wifi, LV_ALIGN_TOP_RIGHT, -8, 6);
    }

    // ============================================================
    //  SOC-Block: grosser zentraler Arc, keine Label drunter
    // ============================================================
    void build_soc_block() {
        // Arc: 180x180, Oberkante y=25 -> Unterkante y=205
        _arc = lv_arc_create(_root);
        lv_obj_set_size(_arc, 180, 180);
        lv_obj_align(_arc, LV_ALIGN_TOP_MID, 0, 25);

        lv_arc_set_bg_angles(_arc, 135, 45);
        lv_arc_set_rotation(_arc, 0);
        lv_arc_set_range(_arc, 0, 100);
        lv_arc_set_value(_arc, 0);

        lv_obj_remove_style(_arc, nullptr, LV_PART_KNOB);
        lv_obj_clear_flag(_arc, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_set_style_arc_color(_arc, lv_color_hex(theme::ARC_BG), LV_PART_MAIN);
        lv_obj_set_style_arc_width(_arc, 18, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(_arc, true, LV_PART_MAIN);

        lv_obj_set_style_arc_color(_arc, lv_color_hex(theme::GOOD), LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(_arc, 18, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(_arc, true, LV_PART_INDICATOR);

        lv_obj_set_style_bg_opa(_arc, LV_OPA_0, LV_PART_MAIN);

        // SOC gross in der Mitte (72pt koennen wir nicht, also 48 + Offset)
        _lbl_soc = lv_label_create(_arc);
        lv_obj_set_style_text_font(_lbl_soc, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(_lbl_soc, lv_color_hex(theme::GOOD), 0);
        lv_label_set_text(_lbl_soc, "0");
        lv_obj_align(_lbl_soc, LV_ALIGN_CENTER, 0, -8);

        // "%" klein drunter
        lv_obj_t* pct = lv_label_create(_arc);
        lv_obj_set_style_text_font(pct, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(pct, lv_color_hex(theme::ACCENT), 0);
        lv_label_set_text(pct, "%");
        lv_obj_align(pct, LV_ALIGN_CENTER, 0, 35);
    }

    // ============================================================
    //  Power-Block: PV-Leistung links-mitte, Einheit rechts,
    //  Batteriestrom ganz rechts (klein). Darunter Sued/West.
    // ============================================================
    void build_power_block() {
        // PV-Zahl: linksbuendig bei x=40 (nicht ganz links, damit
        // kleinere Zahlen optisch zentrierter wirken)
        _lbl_power = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_power, &lv_font_montserrat_40, 0);
        lv_obj_set_style_text_color(_lbl_power, lv_color_hex(theme::PV), 0);
        lv_label_set_text(_lbl_power, "0");
        lv_obj_set_pos(_lbl_power, 40, 188);

        _lbl_power_unit = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_power_unit, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(_lbl_power_unit, lv_color_hex(theme::PV), 0);
        lv_label_set_text(_lbl_power_unit, "W");
        // Provisorische Position rechts neben der Zahl, damit das Label
        // nicht bei (0,0) = oben links ueber der Uhrzeit haengt, bevor
        // update() das erste Mal via align_to nachpositioniert.
        lv_obj_align_to(_lbl_power_unit, _lbl_power,
                        LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -4);

        // Batteriestrom rechts (klein, mit Pfeil) - feste Position am rechten Rand
        _lbl_current = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_current, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_lbl_current, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(_lbl_current, "-- A");
        lv_obj_align(_lbl_current, LV_ALIGN_TOP_RIGHT, -10, 206);

        // Sued/West darunter
        _lbl_sued = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_sued, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(_lbl_sued, lv_color_hex(theme::PV), 0);
        lv_label_set_text(_lbl_sued, "Sued: --");
        lv_obj_set_pos(_lbl_sued, 12, 242);

        _lbl_west = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_west, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(_lbl_west, lv_color_hex(theme::PV), 0);
        lv_label_set_text(_lbl_west, "West: --");
        lv_obj_set_pos(_lbl_west, 128, 242);
    }

    // ============================================================
    //  Footer
    // ============================================================
    void build_footer() {
        _lbl_grid = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_grid, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_lbl_grid, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(_lbl_grid, LV_SYMBOL_REFRESH "  0 W");
        lv_obj_set_pos(_lbl_grid, 12, 278);

        _lbl_load = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_load, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_lbl_load, lv_color_hex(theme::TEXT), 0);
        lv_label_set_text(_lbl_load, LV_SYMBOL_HOME "  0 W");
        lv_obj_set_pos(_lbl_load, 128, 278);

        _lbl_day = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_day, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_lbl_day, lv_color_hex(theme::PV), 0);
        lv_label_set_text(_lbl_day, LV_SYMBOL_CHARGE "  0.0 kWh");
        lv_obj_set_pos(_lbl_day, 12, 302);

        _lbl_export = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_export, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_lbl_export, lv_color_hex(theme::BAD), 0);
        lv_label_set_text(_lbl_export, LV_SYMBOL_DOWNLOAD "  0.0 kWh");
        lv_obj_set_pos(_lbl_export, 128, 302);
    }
};