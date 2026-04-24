#pragma once
// ============================================================
//  Settings-Seite — dritte Tile im Tileview
//
//  Elemente (von oben nach unten):
//    - Titel "Einstellungen"
//    - Auto-Rotate Switch (ein/aus)
//    - Rotate-Intervall Slider (3-60 Sekunden)
//    - Tag-Helligkeit Slider (0-255, live-Vorschau)
//    - Nacht-Helligkeit Slider (0-255, live-Vorschau)
//    - Setup-AP Neustart Button (2s lang druecken)
//    - Info-Panel: IP, RSSI, Uptime, RAM frei
//
//  Slider-Verhalten Helligkeit:
//    - Waehrend gezogen wird: Display zeigt Slider-Wert live
//    - 3s nach letzter Beruehrung: Automatik uebernimmt wieder
//    - Bei Loslassen: Wert in NVS gespeichert
//
//  Die Seite ist innerhalb der Tile vertikal scrollbar, falls
//  der Inhalt hoeher als 320px wird.
// ============================================================
#include <lvgl.h>
#include <Arduino.h>
#include <WiFi.h>

#include "ui_theme.h"
#include "wifi_manager.h"

// Vorwaerts-Typ fuers Callback in den Event-Handlern
class UiSettings;

class UiSettings {
public:
    // Typ fuer Callbacks, die an main.cpp weitergeben koennen.
    using Callback      = void (*)();
    using BrightCallback = void (*)(uint8_t value, bool is_day);

    // Bei Setup-AP-Neustart
    Callback       on_restart_to_ap = nullptr;
    // Bei Slider-Bewegung (fuer Live-Vorschau der Helligkeit)
    BrightCallback on_bright_preview = nullptr;
    // Wenn NVS-Settings gespeichert werden sollen
    Callback       on_save_settings = nullptr;

    // Pointer auf die Settings-Struct, damit Werte gelesen/
    // geschrieben werden koennen.
    Settings* settings = nullptr;

    void create(lv_obj_t* parent, Settings* s) {
        theme_apply_root(parent);
        _root     = parent;
        settings  = s;

        // Tile scrollbar machen (falls Inhalt groesser als 320px)
        lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_scroll_dir(parent, LV_DIR_VER);
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);

        int y = 8;
        y = build_top_bar(y);
        y = build_rotate_slider(y);
        y = build_bright_day_slider(y);
        y = build_bright_night_slider(y);
        y = build_info_panel(y);
    }

    // Muss im Loop von main.cpp aufgerufen werden (1 Hz reicht).
    // Aktualisiert Info-Labels und verwaltet den 3s-Idle-Timeout
    // fuer die Slider-Vorschau.
    void tick() {
        // Info-Texte neu rendern
        update_info();

        // Slider-Vorschau-Timeout: nach 3s idle signalisieren,
        // dass die Automatik wieder uebernehmen darf.
        if (_preview_active && millis() - _t_preview > 3000) {
            _preview_active = false;
            // Main.cpp erkennt an preview_active_flag() == false,
            // dass es wieder auto-brightness setzen darf.
        }
    }

    bool preview_active() const { return _preview_active; }

private:
    lv_obj_t* _root              = nullptr;

    lv_obj_t* _sw_rotate         = nullptr;
    lv_obj_t* _sl_interval       = nullptr;
    lv_obj_t* _lbl_interval_val  = nullptr;
    lv_obj_t* _sl_bright_day     = nullptr;
    lv_obj_t* _lbl_bright_day    = nullptr;
    lv_obj_t* _sl_bright_night   = nullptr;
    lv_obj_t* _lbl_bright_night  = nullptr;
    lv_obj_t* _btn_restart       = nullptr;
    lv_obj_t* _lbl_btn_restart   = nullptr;

    lv_obj_t* _lbl_ip            = nullptr;
    lv_obj_t* _lbl_rssi          = nullptr;
    lv_obj_t* _lbl_uptime        = nullptr;
    lv_obj_t* _lbl_ram           = nullptr;
    lv_obj_t* _lbl_flash         = nullptr;
    lv_obj_t* _lbl_temp          = nullptr;

    // Slider-Vorschau-State
    bool          _preview_active = false;
    unsigned long _t_preview      = 0;

    // Setup-AP-Button Lang-Drueck-State
    unsigned long _t_btn_press    = 0;
    bool          _btn_held       = false;

    // ============================================================
    //  Obere Kompakt-Leiste: AR-Switch links, Setup-AP-Btn rechts
    // ============================================================
    int build_top_bar(int y) {
        // "AR" Label links
        lv_obj_t* lbl = lv_label_create(_root);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(theme::TEXT), 0);
        lv_label_set_text(lbl, "AR");
        lv_obj_set_pos(lbl, 12, y + 4);

        // Switch direkt daneben
        _sw_rotate = lv_switch_create(_root);
        lv_obj_set_size(_sw_rotate, 44, 22);
        lv_obj_set_pos(_sw_rotate, 44, y + 3);
        lv_obj_set_style_bg_color(_sw_rotate,
            lv_color_hex(theme::GOOD), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (settings && settings->auto_rotate) {
            lv_obj_add_state(_sw_rotate, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(_sw_rotate, sw_rotate_cb,
                            LV_EVENT_VALUE_CHANGED, this);

        // Setup-AP-Button rechts (kompakt)
        _btn_restart = lv_btn_create(_root);
        lv_obj_set_size(_btn_restart, 120, 28);
        lv_obj_align(_btn_restart, LV_ALIGN_TOP_RIGHT, -12, y);
        lv_obj_set_style_bg_color(_btn_restart,
            lv_color_hex(theme::BAD), 0);
        lv_obj_set_style_bg_color(_btn_restart,
            lv_color_hex(0xFF8080), LV_STATE_PRESSED);
        lv_obj_set_style_radius(_btn_restart, 6, 0);
        lv_obj_set_style_pad_all(_btn_restart, 0, 0);

        _lbl_btn_restart = lv_label_create(_btn_restart);
        lv_obj_set_style_text_font(_lbl_btn_restart,
            &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(_lbl_btn_restart,
            lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(_lbl_btn_restart, "Setup-AP (2s)");
        lv_obj_center(_lbl_btn_restart);

        lv_obj_add_event_cb(_btn_restart, btn_press_cb,
                            LV_EVENT_PRESSED,  this);
        lv_obj_add_event_cb(_btn_restart, btn_release_cb,
                            LV_EVENT_RELEASED, this);
        lv_obj_add_event_cb(_btn_restart, btn_release_cb,
                            LV_EVENT_PRESS_LOST, this);

        return y + 36;
    }

    static void sw_rotate_cb(lv_event_t* e) {
        UiSettings* self = (UiSettings*)lv_event_get_user_data(e);
        if (!self || !self->settings) return;
        self->settings->auto_rotate =
            lv_obj_has_state(self->_sw_rotate, LV_STATE_CHECKED);
        if (self->on_save_settings) self->on_save_settings();
    }

    // ============================================================
    //  Rotate-Intervall Slider (3-20s)
    // ============================================================
    int build_rotate_slider(int y) {
        lv_obj_t* lbl = lv_label_create(_root);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(lbl, "Rotate-Intervall");
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 12, y);

        _lbl_interval_val = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_interval_val, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(_lbl_interval_val,
            lv_color_hex(theme::ACCENT), 0);
        lv_obj_align(_lbl_interval_val, LV_ALIGN_TOP_RIGHT, -12, y);

        _sl_interval = lv_slider_create(_root);
        lv_obj_set_size(_sl_interval, 216, 10);
        lv_obj_align(_sl_interval, LV_ALIGN_TOP_MID, 0, y + 18);
        lv_slider_set_range(_sl_interval, 3, 20);

        uint8_t val = settings ? settings->rotate_secs : 10;
        if (val < 3) val = 3;
        lv_slider_set_value(_sl_interval, val, LV_ANIM_OFF);
        update_interval_label(val);

        lv_obj_add_event_cb(_sl_interval, sl_interval_cb,
                            LV_EVENT_VALUE_CHANGED, this);
        lv_obj_add_event_cb(_sl_interval, sl_interval_released_cb,
                            LV_EVENT_RELEASED, this);
        return y + 38;
    }

    static void sl_interval_cb(lv_event_t* e) {
        UiSettings* self = (UiSettings*)lv_event_get_user_data(e);
        if (!self) return;
        int v = lv_slider_get_value(self->_sl_interval);
        self->update_interval_label(v);
    }
    static void sl_interval_released_cb(lv_event_t* e) {
        UiSettings* self = (UiSettings*)lv_event_get_user_data(e);
        if (!self || !self->settings) return;
        self->settings->rotate_secs =
            (uint8_t)lv_slider_get_value(self->_sl_interval);
        if (self->on_save_settings) self->on_save_settings();
    }
    void update_interval_label(int v) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d s", v);
        lv_label_set_text(_lbl_interval_val, buf);
    }

    // ============================================================
    //  Tag-Helligkeit Slider (0-255) mit Live-Vorschau
    // ============================================================
    int build_bright_day_slider(int y) {
        _lbl_bright_day = make_bright_slider(
            y, "Helligkeit Tag", &_sl_bright_day,
            settings ? settings->bright_max : 220,
            bright_day_cb, bright_day_released_cb);
        return y + 38;
    }
    static void bright_day_cb(lv_event_t* e) {
        UiSettings* self = (UiSettings*)lv_event_get_user_data(e);
        if (!self) return;
        int v = lv_slider_get_value(self->_sl_bright_day);
        self->update_bright_label(self->_lbl_bright_day, v);
        self->_t_preview = millis();
        self->_preview_active = true;
        if (self->on_bright_preview) self->on_bright_preview((uint8_t)v, true);
    }
    static void bright_day_released_cb(lv_event_t* e) {
        UiSettings* self = (UiSettings*)lv_event_get_user_data(e);
        if (!self || !self->settings) return;
        self->settings->bright_max =
            (uint8_t)lv_slider_get_value(self->_sl_bright_day);
        if (self->on_save_settings) self->on_save_settings();
    }

    // ============================================================
    //  Nacht-Helligkeit Slider (0-255) mit Live-Vorschau
    // ============================================================
    int build_bright_night_slider(int y) {
        _lbl_bright_night = make_bright_slider(
            y, "Helligkeit Nacht", &_sl_bright_night,
            settings ? settings->bright_min : 15,
            bright_night_cb, bright_night_released_cb);
        return y + 38;
    }
    static void bright_night_cb(lv_event_t* e) {
        UiSettings* self = (UiSettings*)lv_event_get_user_data(e);
        if (!self) return;
        int v = lv_slider_get_value(self->_sl_bright_night);
        self->update_bright_label(self->_lbl_bright_night, v);
        self->_t_preview = millis();
        self->_preview_active = true;
        if (self->on_bright_preview) self->on_bright_preview((uint8_t)v, false);
    }
    static void bright_night_released_cb(lv_event_t* e) {
        UiSettings* self = (UiSettings*)lv_event_get_user_data(e);
        if (!self || !self->settings) return;
        self->settings->bright_min =
            (uint8_t)lv_slider_get_value(self->_sl_bright_night);
        if (self->on_save_settings) self->on_save_settings();
    }

    // Helper: generischer Slider mit Caption + Wert-Label
    lv_obj_t* make_bright_slider(int y, const char* caption,
                                  lv_obj_t** out_slider, int start,
                                  lv_event_cb_t cb_change,
                                  lv_event_cb_t cb_release) {
        lv_obj_t* cap = lv_label_create(_root);
        lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(cap, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(cap, caption);
        lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 12, y);

        lv_obj_t* val_lbl = lv_label_create(_root);
        lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(val_lbl,
            lv_color_hex(theme::ACCENT), 0);
        lv_obj_align(val_lbl, LV_ALIGN_TOP_RIGHT, -12, y);

        lv_obj_t* s = lv_slider_create(_root);
        lv_obj_set_size(s, 216, 10);
        lv_obj_align(s, LV_ALIGN_TOP_MID, 0, y + 18);
        lv_slider_set_range(s, 5, 255);
        lv_slider_set_value(s, start, LV_ANIM_OFF);

        char buf[8];
        snprintf(buf, sizeof(buf), "%d", start);
        lv_label_set_text(val_lbl, buf);

        lv_obj_add_event_cb(s, cb_change, LV_EVENT_VALUE_CHANGED, this);
        lv_obj_add_event_cb(s, cb_release, LV_EVENT_RELEASED, this);

        *out_slider = s;
        return val_lbl;
    }

    void update_bright_label(lv_obj_t* lbl, int v) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", v);
        lv_label_set_text(lbl, buf);
    }

    // ============================================================
    // ============================================================
    //  Setup-AP-Button Event-Handler (Button selbst in build_top_bar)
    // ============================================================
    static void btn_press_cb(lv_event_t* e) {
        UiSettings* self = (UiSettings*)lv_event_get_user_data(e);
        if (!self) return;
        self->_t_btn_press = millis();
        self->_btn_held = true;
        lv_label_set_text(self->_lbl_btn_restart, "halten...");
    }

    static void btn_release_cb(lv_event_t* e) {
        UiSettings* self = (UiSettings*)lv_event_get_user_data(e);
        if (!self) return;
        unsigned long dur = millis() - self->_t_btn_press;
        self->_btn_held = false;
        if (dur >= 2000) {
            // Ausreichend gehalten → Restart triggern
            if (self->on_restart_to_ap) self->on_restart_to_ap();
        } else {
            // Zu kurz → Label zuruecksetzen
            lv_label_set_text(self->_lbl_btn_restart,
                              "Setup-AP (2s)");
        }
    }

    // ============================================================
    //  Info-Panel: IP, RSSI, Uptime, RAM frei
    // ============================================================
    int build_info_panel(int y) {
        // Trennstrich
        static const lv_point_t line_points[] = {{20, 0}, {220, 0}};
        lv_obj_t* line = lv_line_create(_root);
        lv_line_set_points(line, line_points, 2);
        lv_obj_set_style_line_color(line,
            lv_color_hex(theme::SURFACE_HI), 0);
        lv_obj_set_style_line_width(line, 1, 0);
        lv_obj_set_pos(line, 0, y + 8);

        int info_y = y + 18;
        _lbl_ip     = info_row("IP",     info_y);
        _lbl_rssi   = info_row("WiFi",   info_y + 20);
        _lbl_uptime = info_row("Uptime", info_y + 40);
        _lbl_ram    = info_row("RAM",    info_y + 60);
        _lbl_flash  = info_row("Flash",  info_y + 80);

        return info_y + 100;
    }

    lv_obj_t* info_row(const char* caption, int y) {
        lv_obj_t* cap = lv_label_create(_root);
        lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(cap,
            lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(cap, caption);
        lv_obj_set_pos(cap, 12, y);

        lv_obj_t* val = lv_label_create(_root);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(val, lv_color_hex(theme::TEXT), 0);
        lv_label_set_text(val, "--");
        lv_obj_align(val, LV_ALIGN_TOP_RIGHT, -12, y);
        return val;
    }

    // Laufende Aktualisierung des Info-Panels
    void update_info() {
        char buf[64];

        if (_lbl_ip) {
            if (WiFi.status() == WL_CONNECTED) {
                snprintf(buf, sizeof(buf), "%s",
                         WiFi.localIP().toString().c_str());
            } else {
                snprintf(buf, sizeof(buf), "offline");
            }
            lv_label_set_text(_lbl_ip, buf);
        }

        if (_lbl_rssi) {
            if (WiFi.status() == WL_CONNECTED) {
                snprintf(buf, sizeof(buf), "%d dBm", WiFi.RSSI());
            } else {
                snprintf(buf, sizeof(buf), "--");
            }
            lv_label_set_text(_lbl_rssi, buf);
        }

        if (_lbl_uptime) {
            unsigned long s = millis() / 1000;
            unsigned long d = s / 86400;  s %= 86400;
            unsigned long h = s / 3600;   s %= 3600;
            unsigned long m = s / 60;     s %= 60;
            if (d > 0) {
                snprintf(buf, sizeof(buf), "%lud %02lu:%02lu", d, h, m);
            } else {
                snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
            }
            lv_label_set_text(_lbl_uptime, buf);
        }

        if (_lbl_ram) {
            uint32_t free_kb = ESP.getFreeHeap() / 1024;
            snprintf(buf, sizeof(buf), "%lu KB", (unsigned long)free_kb);
            lv_label_set_text(_lbl_ram, buf);
        }

        if (_lbl_flash) {
            // Sketch belegt vs. verfuegbare Partition
            uint32_t used_kb  = ESP.getSketchSize()       / 1024;
            uint32_t free_kb2 = ESP.getFreeSketchSpace()  / 1024;
            uint32_t total_kb = used_kb + free_kb2;
            uint32_t pct      = total_kb > 0
                                ? (used_kb * 100) / total_kb : 0;
            snprintf(buf, sizeof(buf), "%lu/%lu KB (%lu%%)",
                     (unsigned long)used_kb,
                     (unsigned long)total_kb,
                     (unsigned long)pct);
            lv_label_set_text(_lbl_flash, buf);
        }
    }

    // ============================================================
    //  Helper: Zeile als transparenter Container
    // ============================================================
    lv_obj_t* row_container(int y, int h) {
        lv_obj_t* c = lv_obj_create(_root);
        lv_obj_set_size(c, 216, h);
        lv_obj_align(c, LV_ALIGN_TOP_MID, 0, y);
        lv_obj_set_style_bg_opa(c, LV_OPA_0, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        return c;
    }
};