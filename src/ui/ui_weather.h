#pragma once
// ============================================================
//  Wetter-Seite — LVGL
//
//  Home-Assistant-Weather-Conditions als Eingabe.
//  Icons sind 96x96 TRUE_COLOR_ALPHA-Bitmaps, hochskaliert
//  aus den Original-TFT_eSPI-Icons. Die weissen Pixel werden
//  via image_recolor pro Condition eingefaerbt.
//
//  Layout (240x320):
//  - Header  y=  0.. 22  Clock + WiFi
//  - Icon    y= 30..126  96x96 Bitmap (Sonne darunter bei
//                        partlycloudy als 2. Layer)
//  - Temp    y=135..183  Haupttemperatur, 48pt, farbig nach Wert
//  - Cond    y=192..212  Text "Sonnig", "Regen", ...
//  - Divider y=220
//  - Grid    y=232..315  2x2 Block: Feuchte, Wind, Druck,
//                        Richtung (jedes Tile eigene Akzentfarbe)
// ============================================================
#include <lvgl.h>
#include <Arduino.h>
#include <time.h>

#include "ui_theme.h"
#include "weather_icons_lv_96tc.h"
#include "weather_data.h"

class UiWeather {
public:
    void create(lv_obj_t* parent) {
        theme_apply_root(parent);
        _root = parent;

        build_header();
        build_icon();
        build_main();
        build_grid();
    }

    void update(const WeatherData& w) {
        char buf[48];

        // Condition → Icon (Bitmap) + Farbe + Label
        const char* cond = w.condition;
        const lv_img_dsc_t* img;
        uint32_t    col;
        const char* label;
        bool        show_sun;
        condition_to_visuals(cond, img, col, label, show_sun);

        lv_img_set_src(_img_icon, img);
        lv_obj_set_style_img_recolor(_img_icon, lv_color_hex(col), 0);
        lv_label_set_text(_lbl_cond, label);

        // Sonnen-Overlay fuer partlycloudy ein-/ausblenden
        if (show_sun) lv_obj_clear_flag(_img_sun, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag  (_img_sun, LV_OBJ_FLAG_HIDDEN);

        // Temperatur mit Farbskala: kalt=blau, lau=weiss, warm=gelb, heiss=rot
        if (isnanf(w.temperature)) {
            lv_label_set_text(_lbl_temp, "--");
            lv_obj_set_style_text_color(_lbl_temp, lv_color_hex(theme::TEXT_MUTED), 0);
            lv_obj_set_style_text_color(_lbl_deg,  lv_color_hex(theme::TEXT_MUTED), 0);
        } else {
            snprintf(buf, sizeof(buf), "%.1f", w.temperature);
            lv_label_set_text(_lbl_temp, buf);
            uint32_t tcol = temp_color(w.temperature);
            lv_obj_set_style_text_color(_lbl_temp, lv_color_hex(tcol), 0);
            lv_obj_set_style_text_color(_lbl_deg,  lv_color_hex(tcol), 0);
        }

        // Feuchte
        if (isnanf(w.humidity)) lv_label_set_text(_val_hum, "-- %");
        else { snprintf(buf, sizeof(buf), "%.0f %%", w.humidity);
               lv_label_set_text(_val_hum, buf); }

        // Wind
        if (isnanf(w.wind_speed)) lv_label_set_text(_val_wind, "-- km/h");
        else { snprintf(buf, sizeof(buf), "%.0f km/h", w.wind_speed);
               lv_label_set_text(_val_wind, buf); }

        // Druck
        if (isnanf(w.pressure)) lv_label_set_text(_val_press, "-- hPa");
        else { snprintf(buf, sizeof(buf), "%.0f hPa", w.pressure);
               lv_label_set_text(_val_press, buf); }

        // Windrichtung als Kompass-Text
        if (isnanf(w.wind_bearing)) {
            lv_label_set_text(_val_dir, "--");
        } else {
            lv_label_set_text(_val_dir, bearing_to_compass(w.wind_bearing));
        }
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
    lv_obj_t* _root       = nullptr;
    lv_obj_t* _lbl_clock  = nullptr;
    lv_obj_t* _lbl_wifi   = nullptr;
    lv_obj_t* _img_icon   = nullptr;
    lv_obj_t* _img_sun    = nullptr;   // Sonnen-Overlay fuer partlycloudy
    lv_obj_t* _lbl_temp   = nullptr;
    lv_obj_t* _lbl_deg    = nullptr;
    lv_obj_t* _lbl_cond   = nullptr;
    lv_obj_t* _val_hum    = nullptr;
    lv_obj_t* _val_wind   = nullptr;
    lv_obj_t* _val_press  = nullptr;
    lv_obj_t* _val_dir    = nullptr;

    // ============================================================
    //  Condition → Bitmap-Icon / Farbe / deutsche Bezeichnung
    //  show_sun = true: Sonne als 2. Layer hinter das Icon legen
    //                   (derzeit nur fuer partlycloudy)
    // ============================================================
    void condition_to_visuals(const char* cond, const lv_img_dsc_t*& img,
                              uint32_t& col, const char*& label,
                              bool& show_sun) {
        // Defaults
        img      = &w_cloudy;
        col      = theme::TEXT_MUTED;
        label    = "Unbekannt";
        show_sun = false;

        if (!cond) return;

        // Sonnig / klar
        if (!strcmp(cond, "sunny") || !strcmp(cond, "clear")) {
            img = &w_sunny;
            col = theme::PV;
            label = "Sonnig";
        }
        else if (!strcmp(cond, "clear-night")) {
            img = &w_clear_night;
            col = theme::ACCENT;
            label = "Klare Nacht";
        }
        // Bewoelkt - mit Sonnen-Overlay
        else if (!strcmp(cond, "partlycloudy")) {
            img = &w_cloudy;
            col = theme::TEXT_DIM;
            label = "Teils bewoelkt";
            show_sun = true;
        }
        else if (!strcmp(cond, "cloudy") || !strcmp(cond, "overcast")) {
            img = &w_cloudy;
            col = theme::TEXT_MUTED;
            label = "Bewoelkt";
        }
        // Regen
        else if (!strcmp(cond, "rainy") || !strcmp(cond, "pouring")) {
            img = &w_rain;
            col = theme::ACCENT;
            label = !strcmp(cond, "pouring") ? "Starkregen" : "Regen";
        }
        // Gewitter
        else if (!strcmp(cond, "lightning-rainy") || !strcmp(cond, "lightning")) {
            img = &w_thunder;
            col = theme::PV;
            label = "Gewitter";
        }
        // Schnee
        else if (!strcmp(cond, "snowy") || !strcmp(cond, "snowy-rainy")) {
            img = &w_snow;
            col = theme::TEXT;
            label = !strcmp(cond, "snowy-rainy") ? "Schneeregen" : "Schnee";
        }
        else if (!strcmp(cond, "fog")) {
            img = &w_cloudy;
            col = theme::TEXT_MUTED;
            label = "Nebel";
        }
        else if (!strcmp(cond, "windy")) {
            img = &w_cloudy;
            col = theme::ACCENT;
            label = "Windig";
        }
        else if (!strcmp(cond, "hail")) {
            img = &w_snow;
            col = theme::TEXT;
            label = "Hagel";
        }
    }

    // Windrichtung in Grad → Kompass-Richtung N, NE, E, ...
    const char* bearing_to_compass(float deg) {
        static const char* dirs[8] = {"N", "NO", "O", "SO", "S", "SW", "W", "NW"};
        int idx = (int)((deg + 22.5f) / 45.0f) & 7;
        return dirs[idx];
    }

    // Temperatur → Farbcode
    //   < 0 C  : tiefes Blau (Frost)
    //   0-12 C : Blau (kalt)
    //   12-22 C: Gruen (angenehm)
    //   22-28 C: Gelb (warm)
    //   > 28 C : Rot (heiss)
    uint32_t temp_color(float t) const {
        if (t < 0.0f)  return 0x4A9EFF;       // kaltes Blau
        if (t < 12.0f) return theme::ACCENT;  // cyan
        if (t < 22.0f) return theme::GOOD;    // gruen
        if (t < 28.0f) return theme::PV;      // gelb
        return theme::BAD;                    // rot
    }

    // ============================================================
    //  Header (wie Solar-Seite)
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
    //  Grosses Wetter-Icon (96x96 TRUE_COLOR_ALPHA)
    //  Kein Zoom, kein 1bpp-Trick - einfach rendern.
    //  Die Bitmap-Pixel sind weiss, die gewuenschte Farbe kommt
    //  per image_recolor_opa drauf.
    //
    //  Zweilagig: Sonne unten (standardmaessig hidden), darueber
    //  das Haupt-Icon. Bei "partlycloudy" wird die Sonne gelb
    //  eingeblendet und nach rechts oben versetzt, sodass sie
    //  hinter der Wolke hervorschaut.
    // ============================================================
    void build_icon() {
        // Sonnen-Overlay (hinten, zunaechst versteckt)
        _img_sun = lv_img_create(_root);
        lv_img_set_src(_img_sun, &w_sunny);
        lv_obj_set_style_img_recolor(_img_sun,
            lv_color_hex(theme::PV), 0);
        lv_obj_set_style_img_recolor_opa(_img_sun, LV_OPA_COVER, 0);
        // Etwas nach oben-rechts versetzt, sodass sie hinter der
        // Wolke hervorschaut (wenn sichtbar).
        lv_obj_align(_img_sun, LV_ALIGN_TOP_MID, 24, 18);
        lv_obj_add_flag(_img_sun, LV_OBJ_FLAG_HIDDEN);

        // Haupt-Icon darueber (weil spaeter erzeugt → hoehere Z-Order)
        _img_icon = lv_img_create(_root);
        lv_img_set_src(_img_icon, &w_cloudy);
        lv_obj_set_style_img_recolor(_img_icon,
            lv_color_hex(theme::TEXT_MUTED), 0);
        lv_obj_set_style_img_recolor_opa(_img_icon, LV_OPA_COVER, 0);
        lv_obj_align(_img_icon, LV_ALIGN_TOP_MID, 0, 30);
    }

    // ============================================================
    //  Haupttemperatur + Condition-Label
    // ============================================================
    void build_main() {
        _lbl_temp = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_temp, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(_lbl_temp, lv_color_hex(theme::TEXT), 0);
        lv_label_set_text(_lbl_temp, "--");
        // y=135, damit unter dem 96px-Icon Platz ist (Icon y=30..126)
        lv_obj_align(_lbl_temp, LV_ALIGN_TOP_MID, -12, 135);

        _lbl_deg = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_deg, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(_lbl_deg, lv_color_hex(theme::TEXT_DIM), 0);
        lv_label_set_text(_lbl_deg, "\xC2\xB0""C");
        lv_obj_align_to(_lbl_deg, _lbl_temp, LV_ALIGN_OUT_RIGHT_TOP, 26, 8);

        // Condition-Label darunter
        _lbl_cond = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_cond, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(_lbl_cond, lv_color_hex(theme::TEXT_DIM), 0);
        lv_label_set_text(_lbl_cond, "Warte auf Daten");
        lv_obj_align(_lbl_cond, LV_ALIGN_TOP_MID, 0, 192);
    }

    // ============================================================
    //  2x2-Grid: Feuchte, Wind, Druck, Windrichtung
    //  Jedes Tile bekommt eine eigene Akzentfarbe zur besseren
    //  visuellen Unterscheidung.
    // ============================================================
    void build_grid() {
        // Trennstrich
        static const lv_point_t line_points[] = {{20, 0}, {220, 0}};
        lv_obj_t* line = lv_line_create(_root);
        lv_line_set_points(line, line_points, 2);
        lv_obj_set_style_line_color(line, lv_color_hex(theme::SURFACE_HI), 0);
        lv_obj_set_style_line_width(line, 1, 0);
        lv_obj_set_pos(line, 0, 220);

        add_tile("Feuchte",   &_val_hum,   12,  232, LV_SYMBOL_TINT,    theme::ACCENT);
        add_tile("Wind",      &_val_wind, 128,  232, LV_SYMBOL_REFRESH, theme::GOOD);
        add_tile("Druck",     &_val_press, 12,  275, LV_SYMBOL_MINUS,   theme::MID);
        add_tile("Richtung",  &_val_dir,  128,  275, LV_SYMBOL_UP,      theme::PV);
    }

    void add_tile(const char* caption, lv_obj_t** out_val,
                  int x, int y, const char* icon_sym, uint32_t accent) {
        // Caption oben: Icon eingefaerbt, Text gedaempft
        lv_obj_t* cap = lv_label_create(_root);
        lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
        // LVGL "recolor" in Labels: #RRGGBB Text # setzt Farbe
        lv_label_set_recolor(cap, true);
        char buf[48];
        snprintf(buf, sizeof(buf), "#%06X %s# %s",
                 (unsigned)(accent & 0xFFFFFF), icon_sym, caption);
        lv_label_set_text(cap, buf);
        lv_obj_set_style_text_color(cap, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_obj_set_pos(cap, x, y);

        // Wert darunter in Akzentfarbe
        lv_obj_t* val = lv_label_create(_root);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(val, lv_color_hex(accent), 0);
        lv_label_set_text(val, "--");
        lv_obj_set_pos(val, x, y + 16);
        *out_val = val;
    }
};