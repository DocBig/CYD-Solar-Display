#pragma once
// ============================================================
//  Energiefluss-Diagramm — Seite 3 (Energiebilanz-Variante)
//
//  Visualisierung mit Quellfarben:
//   - 5 Pfeile pro Strecke = 100% Hausverbrauch
//   - Pfeile werden in der Farbe der QUELLE eingefaerbt:
//        gelb = PV, gruen = Akku, rot = Netz
//   - Auf Hub->Haus erscheinen Mischfarben je nach Aufteilung
//        z.B. 3 gelbe + 1 gruener + 1 roter Pfeil
//
//  Verteilungs-Konvention "Erneuerbar zuerst":
//   1. PV deckt Hausverbrauch direkt ab
//   2. PV-Ueberschuss laedt Akku
//   3. Wenn Haus mehr braucht als PV: Akku-Entladung deckt nach
//   4. Reicht das immer noch nicht: Netz-Bezug
//   (Netz-Einspeisung bei dir nicht vorgesehen, daher weggelassen)
//
//  Knoten-Symbole sind statisch (kein Pulsieren mehr).
//  Alle sichtbaren Pfeile pulsieren synchron in der Helligkeit.
//
//  Layout (240x320):
//   - Header        y=  0..22   Clock + WiFi
//   - PV-Knoten     y= 25.. 80
//   - Hub           y=145
//   - Grid links    y=110..165
//   - Battery rechts y=110..165
//   - Haus unten    y=210..260
//   - Footer        y=275..318
// ============================================================
#include <lvgl.h>
#include <Arduino.h>
#include <time.h>
#include <cmath>

#include "ui_theme.h"
#include "solar_data.h"

class UiFlow {
public:
    void create(lv_obj_t* parent) {
        theme_apply_root(parent);
        _root = parent;

        build_header();
        build_arrows();
        build_nodes();
        build_footer();
        start_pulse_master();
    }

    void update(const SolarData& d) {
        update_pv_node(d);
        update_battery_node(d);
        update_grid_node(d);
        update_house_node(d);
        update_flow_directions(d);
        update_footer(d);
    }

    void update_clock() {
        struct tm t;
        if (getLocalTime(&t, 10)) {
            char buf[12];
            snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
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
    // ============================================================
    //  Layout-Konstanten
    // ============================================================
    static constexpr lv_coord_t CENTER_X = 120;
    static constexpr lv_coord_t PV_Y     = 55;
    static constexpr lv_coord_t HUB_Y    = 145;
    static constexpr lv_coord_t HOUSE_Y  = 225;
    static constexpr lv_coord_t LEFT_X   = 38;
    static constexpr lv_coord_t RIGHT_X  = 202;

    // 5 Pfeile pro Strecke = 100% Hausverbrauch
    static constexpr int        ARROW_COUNT = 5;

    // Akku-Bus-Spannung fuer Power-Schaetzung aus Strom (Niedervolt-Akku)
    // Bei Hochvolt-Akkus auf 400.0f setzen oder d.bat_power direkt nutzen.
    static constexpr float      BAT_BUS_VOLTAGE = 50.0f;

    // Pulse Timing
    static constexpr uint32_t   PULSE_HALF    = 800;
    static constexpr int32_t    PULSE_OPA_MIN = 60;
    static constexpr int32_t    PULSE_OPA_MAX = 255;

    // ============================================================
    //  Member-Variablen
    // ============================================================
    lv_obj_t* _root      = nullptr;
    lv_obj_t* _lbl_clock = nullptr;
    lv_obj_t* _lbl_wifi  = nullptr;

    // Knoten (statisch)
    lv_obj_t* _node_pv_icon     = nullptr;
    lv_obj_t* _node_pv_value    = nullptr;
    lv_obj_t* _node_grid_icon   = nullptr;
    lv_obj_t* _node_grid_value  = nullptr;
    lv_obj_t* _node_bat_icon    = nullptr;
    lv_obj_t* _node_bat_value   = nullptr;
    lv_obj_t* _node_house_icon  = nullptr;
    lv_obj_t* _node_house_value = nullptr;

    // Pfeil-Sets (5 Pfeile pro Strecke)
    struct ArrowSet {
        lv_obj_t* arrows[ARROW_COUNT] = {nullptr};
    };

    ArrowSet _set_pv_hub;       // PV -> Hub          (immer gelb)
    ArrowSet _set_grid_in;      // Grid -> Hub        (immer rot)
    ArrowSet _set_bat_in;       // Hub -> Akku Laden  (gelb, weil PV-Quelle)
    ArrowSet _set_bat_out;      // Akku -> Hub        (immer gruen)
    ArrowSet _set_hub_house;    // Hub -> Haus        (Mischfarben!)

    // Master-Pulse
    int32_t   _pulse_opa = PULSE_OPA_MAX;
    lv_anim_t _anim_pulse_master;

    // Footer
    //lv_obj_t* _lbl_sued   = nullptr;
    //lv_obj_t* _lbl_west   = nullptr;
    lv_obj_t* _lbl_day    = nullptr;
    lv_obj_t* _lbl_import = nullptr;

    // ============================================================
    //  Header
    // ============================================================
    void build_header() {
        _lbl_clock = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_clock, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_lbl_clock, lv_color_hex(theme::ACCENT), 0);
        lv_label_set_text(_lbl_clock, "--:--");
        lv_obj_align(_lbl_clock, LV_ALIGN_TOP_LEFT, 8, 6);

        _lbl_wifi = lv_label_create(_root);
        lv_obj_set_style_text_font(_lbl_wifi, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_lbl_wifi, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(_lbl_wifi, LV_SYMBOL_WIFI);
        lv_obj_align(_lbl_wifi, LV_ALIGN_TOP_RIGHT, -8, 6);
    }

    // ============================================================
    //  Pfeile aufbauen
    // ============================================================
    void build_arrows() {
        // PV -> Hub (vertikal, gelb)
        lv_coord_t pv_top    = PV_Y + 28;
        lv_coord_t pv_bottom = HUB_Y - 18;
        lv_coord_t pv_step   = (pv_bottom - pv_top) / (ARROW_COUNT - 1);
        for (int i = 0; i < ARROW_COUNT; i++) {
            _set_pv_hub.arrows[i] = make_arrow(LV_SYMBOL_DOWN, theme::PV);
            lv_obj_set_pos(_set_pv_hub.arrows[i],
                           CENTER_X - 8, pv_top + i * pv_step - 8);
            lv_obj_add_flag(_set_pv_hub.arrows[i], LV_OBJ_FLAG_HIDDEN);
        }

        // Grid -> Hub (horizontal, rot)
        lv_coord_t grid_right = CENTER_X - 18;
        lv_coord_t grid_left  = LEFT_X + 18;
        lv_coord_t grid_step  = (grid_right - grid_left) / (ARROW_COUNT - 1);
        for (int i = 0; i < ARROW_COUNT; i++) {
            lv_coord_t x = grid_left + i * grid_step;
            _set_grid_in.arrows[i] = make_arrow(LV_SYMBOL_RIGHT, theme::BAD);
            lv_obj_set_pos(_set_grid_in.arrows[i], x - 8, HUB_Y - 10);
            lv_obj_add_flag(_set_grid_in.arrows[i], LV_OBJ_FLAG_HIDDEN);
        }

        // Hub <-> Battery (horizontal, IN=gelb fuer Laden, OUT=gruen fuer Entladen)
        lv_coord_t bat_left  = CENTER_X + 18;
        lv_coord_t bat_right = RIGHT_X - 18;
        lv_coord_t bat_step  = (bat_right - bat_left) / (ARROW_COUNT - 1);
        for (int i = 0; i < ARROW_COUNT; i++) {
            lv_coord_t x = bat_left + i * bat_step;

            // Laden: Pfeil RIGHT in PV-Farbe (gelb), weil Energie aus PV kommt
            _set_bat_in.arrows[i] = make_arrow(LV_SYMBOL_RIGHT, theme::PV);
            lv_obj_set_pos(_set_bat_in.arrows[i], x - 8, HUB_Y - 10);
            lv_obj_add_flag(_set_bat_in.arrows[i], LV_OBJ_FLAG_HIDDEN);

            // Entladen: Pfeil LEFT in GOOD-Farbe (gruen)
            _set_bat_out.arrows[i] = make_arrow(LV_SYMBOL_LEFT, theme::GOOD);
            lv_obj_set_pos(_set_bat_out.arrows[i], x - 8, HUB_Y - 10);
            lv_obj_add_flag(_set_bat_out.arrows[i], LV_OBJ_FLAG_HIDDEN);
        }

        // Hub -> House (vertikal, Mischfarben pro Pfeil!)
        // Initial alle gelb, Farbe wird in update() pro Pfeil gesetzt.
        lv_coord_t hh_top    = HUB_Y + 18;
        lv_coord_t hh_bottom = HOUSE_Y - 15;
        lv_coord_t hh_step   = (hh_bottom - hh_top) / (ARROW_COUNT - 1);
        for (int i = 0; i < ARROW_COUNT; i++) {
            _set_hub_house.arrows[i] = make_arrow(LV_SYMBOL_DOWN, theme::PV);
            lv_obj_set_pos(_set_hub_house.arrows[i],
                           CENTER_X - 8, hh_top + i * hh_step - 8);
            lv_obj_add_flag(_set_hub_house.arrows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_t* make_arrow(const char* sym, uint32_t color) {
        lv_obj_t* a = lv_label_create(_root);
        lv_obj_set_style_text_font(a, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(a, lv_color_hex(color), 0);
        lv_label_set_text(a, sym);
        return a;
    }

    // ============================================================
    //  Knoten aufbauen
    // ============================================================
    void build_nodes() {
        _node_pv_icon = make_label(LV_SYMBOL_CHARGE, theme::PV,
                                   &lv_font_montserrat_28);
        lv_obj_align(_node_pv_icon, LV_ALIGN_TOP_MID, 0, PV_Y - 30);
        _node_pv_value = make_label("0 W", theme::PV,
                                    &lv_font_montserrat_18);
        lv_obj_align(_node_pv_value, LV_ALIGN_TOP_MID, 0, PV_Y);

        _node_grid_icon = make_label(LV_SYMBOL_REFRESH, theme::TEXT_MUTED,
                                     &lv_font_montserrat_24);
        lv_obj_set_pos(_node_grid_icon, LEFT_X - 12, HUB_Y - 38);
        _node_grid_value = make_label("0 W", theme::TEXT_MUTED,
                                      &lv_font_montserrat_14);
        lv_obj_align_to(_node_grid_value, _node_grid_icon,
                        LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

        _node_bat_icon = make_label(LV_SYMBOL_BATTERY_FULL, theme::GOOD,
                                    &lv_font_montserrat_24);
        lv_obj_set_pos(_node_bat_icon, RIGHT_X - 12, HUB_Y - 38);
        _node_bat_value = make_label("0%", theme::GOOD,
                                     &lv_font_montserrat_14);
        lv_obj_align_to(_node_bat_value, _node_bat_icon,
                        LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

        _node_house_icon = make_label(LV_SYMBOL_HOME, theme::ACCENT,
                                      &lv_font_montserrat_28);
        lv_obj_align(_node_house_icon, LV_ALIGN_TOP_MID, 0, HOUSE_Y - 5);
        _node_house_value = make_label("0 W", theme::ACCENT,
                                       &lv_font_montserrat_18);
        lv_obj_align(_node_house_value, LV_ALIGN_TOP_MID, 0, HOUSE_Y + 25);
    }

    lv_obj_t* make_label(const char* txt, uint32_t color,
                         const lv_font_t* font) {
        lv_obj_t* l = lv_label_create(_root);
        lv_obj_set_style_text_font(l, font, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
        lv_label_set_text(l, txt);
        return l;
    }

    // ============================================================
    //  Footer
    // ============================================================
    void build_footer() {
        //_lbl_sued = make_label("Sued: --", theme::GOOD,
        //                       &lv_font_montserrat_14);
        //lv_obj_set_pos(_lbl_sued, 12, 275);
        //_lbl_west = make_label("West: --", theme::GOOD,
        //                       &lv_font_montserrat_14);
        //lv_obj_set_pos(_lbl_west, 128, 275);

        _lbl_day = make_label(LV_SYMBOL_CHARGE "  0.0 kWh", theme::PV,
                              &lv_font_montserrat_16);
        lv_obj_set_pos(_lbl_day, 12, 298);
        _lbl_import = make_label(LV_SYMBOL_DOWNLOAD "  0.0 kWh", theme::BAD,
                                 &lv_font_montserrat_16);
        lv_obj_set_pos(_lbl_import, 128, 298);
    }

    // ============================================================
    //  Knoten-/Footer-Updates
    // ============================================================
    void update_pv_node(const SolarData& d) {
        char buf[32];
        float pv = std::isnan(d.pv_power) ? 0.f : d.pv_power;
        if (pv < 1000) snprintf(buf, sizeof(buf), "%d W", (int)pv);
        else           snprintf(buf, sizeof(buf), "%.2f kW", pv / 1000.0f);
        lv_label_set_text(_node_pv_value, buf);
    }

    void update_battery_node(const SolarData& d) {
        char buf[16];
        float soc = std::isnan(d.bat_soc) ? 0.f : d.bat_soc;
        if (soc < 0)   soc = 0;
        if (soc > 100) soc = 100;
        snprintf(buf, sizeof(buf), "%d%%", (int)soc);
        lv_label_set_text(_node_bat_value, buf);

        const char* icon = LV_SYMBOL_BATTERY_FULL;
        if      (soc < 20) icon = LV_SYMBOL_BATTERY_EMPTY;
        else if (soc < 40) icon = LV_SYMBOL_BATTERY_1;
        else if (soc < 60) icon = LV_SYMBOL_BATTERY_2;
        else if (soc < 85) icon = LV_SYMBOL_BATTERY_3;
        lv_label_set_text(_node_bat_icon, icon);

        uint32_t col = (soc < 20) ? theme::BAD : theme::GOOD;
        lv_obj_set_style_text_color(_node_bat_icon, lv_color_hex(col), 0);
        lv_obj_set_style_text_color(_node_bat_value, lv_color_hex(col), 0);
    }

    void update_grid_node(const SolarData& d) {
        char buf[32];
        float g = std::isnan(d.grid_power) ? 0.f : d.grid_power;
        int abs_g = (int)fabsf(g);
        if (abs_g < 1000) snprintf(buf, sizeof(buf), "%d W", abs_g);
        else              snprintf(buf, sizeof(buf), "%.1f kW", abs_g / 1000.0f);
        lv_label_set_text(_node_grid_value, buf);

        uint32_t col;
        if      (g >  5.0f) col = theme::BAD;
        else if (g < -5.0f) col = theme::GOOD;
        else                col = theme::TEXT_MUTED;
        lv_obj_set_style_text_color(_node_grid_icon, lv_color_hex(col), 0);
        lv_obj_set_style_text_color(_node_grid_value, lv_color_hex(col), 0);
    }

    void update_house_node(const SolarData& d) {
        char buf[32];
        float l = std::isnan(d.load_power) ? 0.f : d.load_power;
        if (l < 1000) snprintf(buf, sizeof(buf), "%d W", (int)l);
        else          snprintf(buf, sizeof(buf), "%.2f kW", l / 1000.0f);
        lv_label_set_text(_node_house_value, buf);
    }

    void update_footer(const SolarData& d) {
        char buf[32];
        //float pv1 = std::isnan(d.pv1_power) ? 0.f : d.pv1_power;
        //float pv2 = std::isnan(d.pv2_power) ? 0.f : d.pv2_power;
        //snprintf(buf, sizeof(buf), "Sued: %dW", (int)pv1);
        //lv_label_set_text(_lbl_sued, buf);
        //snprintf(buf, sizeof(buf), "West: %dW", (int)pv2);
        //lv_label_set_text(_lbl_west, buf);

        float day = std::isnan(d.day_pv) ? 0.f : d.day_pv;
        snprintf(buf, sizeof(buf), LV_SYMBOL_CHARGE "  %.1f kWh", day);
        lv_label_set_text(_lbl_day, buf);

        float imp = std::isnan(d.day_import) ? 0.f : d.day_import;
        snprintf(buf, sizeof(buf), LV_SYMBOL_DOWNLOAD "  %.1f kWh", imp);
        lv_label_set_text(_lbl_import, buf);
    }

    // ============================================================
    //  HERZSTUECK: Energiebilanz + Pfeile mit Quellfarben
    // ============================================================
    void update_flow_directions(const SolarData& d) {
        float pv      = std::isnan(d.pv_power)    ? 0.f : d.pv_power;
        float grid    = std::isnan(d.grid_power)  ? 0.f : d.grid_power;
        float bat_cur = std::isnan(d.bat_current) ? 0.f : d.bat_current;
        float load    = std::isnan(d.load_power)  ? 0.f : d.load_power;

        // Akku-Leistung schaetzen (positiv = Entladen, negativ = Laden bei Sungrow)
        float bat_power = bat_cur * BAT_BUS_VOLTAGE;
        float bat_discharge = (bat_power > 0) ? bat_power : 0.0f;
        float bat_charge    = (bat_power < 0) ? -bat_power : 0.0f;

        // Netz-Bezug (Einspeisung wird ignoriert lt. deiner Vorgabe)
        float grid_import = (grid > 0) ? grid : 0.0f;

        // Bei sehr kleinem Hausverbrauch: alles ausblenden
        if (load < 50.0f) {
            set_count_simple(_set_pv_hub,   0);
            set_count_simple(_set_grid_in,  0);
            set_count_simple(_set_bat_in,   0);
            set_count_simple(_set_bat_out,  0);
            set_count_simple(_set_hub_house, 0);
            return;
        }

        // ── Schritt 1: "Erneuerbar zuerst" — Aufteilung der Quellen aufs Haus ──
        float remaining = load;

        // 1a. PV deckt Haus zuerst
        float pv_to_house = fminf(pv, remaining);
        remaining -= pv_to_house;

        // 1b. Akku-Entladung deckt Rest (falls aktiv)
        float bat_to_house = fminf(bat_discharge, remaining);
        remaining -= bat_to_house;

        // 1c. Netz-Bezug deckt verbleibenden Rest
        float grid_to_house = fminf(grid_import, remaining);
        // remaining sollte jetzt ~0 sein (sonst Bilanz-Fehler oder Datenversatz)

        // ── Schritt 2: PV-Ueberschuss laedt Akku ──
        float pv_excess = pv - pv_to_house;        // was nach Hausdeckung uebrig ist
        float pv_to_bat = fminf(pv_excess, bat_charge);

        // ── Schritt 3: Pfeil-Anzahlen berechnen (Skala = Hausverbrauch) ──
        // Hub->Haus: drei Anteile aufteilen, Summe = ARROW_COUNT
        int n_pv   = arrows_for(pv_to_house,   load);
        int n_bat  = arrows_for(bat_to_house,  load);
        int n_grid = arrows_for(grid_to_house, load);
        balance_to_total(n_pv, n_bat, n_grid, ARROW_COUNT,
                         pv_to_house, bat_to_house, grid_to_house);

        // PV->Hub: Gesamt-PV (an Haus + an Akku) relativ zur Last
        int n_pv_total = arrows_for(pv_to_house + pv_to_bat, load);

        // Akku-Strecken (Laden ODER Entladen, nie beides)
        int n_bat_charge    = arrows_for(pv_to_bat,    load);
        int n_bat_discharge = arrows_for(bat_to_house, load);

        // Netz-Strecke (nur Bezug)
        int n_grid_import = arrows_for(grid_to_house, load);

        // ── Schritt 4: Pfeile setzen ──
        set_count_simple(_set_pv_hub,   n_pv_total);
        set_count_simple(_set_grid_in,  n_grid_import);
        set_count_simple(_set_bat_in,   n_bat_charge);
        set_count_simple(_set_bat_out,  n_bat_discharge);

        // Hub -> Haus mit Mischfarben:
        // Reihenfolge: zuerst PV (gelb, oben), dann Akku (gruen),
        // dann Netz (rot, unten am Haus).
        set_house_arrows_mixed(n_pv, n_bat, n_grid);
    }

    // Pfeil-Anzahl proportional zum Anteil am Hausverbrauch
    int arrows_for(float power, float load) {
        if (load <= 0 || power <= 0) return 0;
        int n = (int)roundf((power / load) * ARROW_COUNT);
        if (n < 0) return 0;
        if (n > ARROW_COUNT) return ARROW_COUNT;
        return n;
    }

    // Korrigiert Rundungsfehler so, dass Summe = ARROW_COUNT.
    // Bei Diskrepanz wird der groesste Anteil bevorzugt aufgestockt
    // bzw. der kleinste gekuerzt.
    void balance_to_total(int& n_pv, int& n_bat, int& n_grid, int target,
                          float p_pv, float p_bat, float p_grid) {
        int sum = n_pv + n_bat + n_grid;
        // Falls keine Quelle aktiv ist: nichts zu tun
        if (sum == 0) return;

        // Auf target hochskalieren / runterkuerzen
        while (sum < target) {
            // Den groessten Anteil aufstocken (der nicht schon am MAX ist)
            if (p_pv >= p_bat && p_pv >= p_grid && n_pv < target)        n_pv++;
            else if (p_bat >= p_grid && n_bat < target)                  n_bat++;
            else if (n_grid < target)                                    n_grid++;
            else break;
            sum = n_pv + n_bat + n_grid;
        }
        while (sum > target) {
            // Den kleinsten Anteil kuerzen (der nicht schon 0 ist)
            if (p_grid > 0 && n_grid > 0 && p_grid <= p_bat && p_grid <= p_pv)  n_grid--;
            else if (p_bat > 0 && n_bat > 0 && p_bat <= p_pv)                   n_bat--;
            else if (n_pv > 0)                                                  n_pv--;
            else break;
            sum = n_pv + n_bat + n_grid;
        }
    }

    // Setzt Hub->Haus-Pfeile mit Mischfarben:
    // Index 0..n_pv-1                          : gelb (PV)
    // Index n_pv..n_pv+n_bat-1                 : gruen (Akku)
    // Index n_pv+n_bat..n_pv+n_bat+n_grid-1    : rot (Netz)
    // Restliche Pfeile (falls Summe < ARROW_COUNT) sind hidden.
    void set_house_arrows_mixed(int n_pv, int n_bat, int n_grid) {
        for (int i = 0; i < ARROW_COUNT; i++) {
            lv_obj_t* arr = _set_hub_house.arrows[i];
            if (!arr) continue;

            uint32_t col;
            bool show = true;
            if      (i < n_pv)               col = theme::PV;
            else if (i < n_pv + n_bat)       col = theme::GOOD;
            else if (i < n_pv + n_bat + n_grid) col = theme::BAD;
            else                              { show = false; col = theme::PV; }

            if (show) {
                lv_obj_set_style_text_color(arr, lv_color_hex(col), 0);
                lv_obj_clear_flag(arr, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(arr, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    // Einfaches Set: erste n Pfeile sichtbar, Rest hidden, Farbe bleibt.
    void set_count_simple(ArrowSet& set, int n) {
        if (n < 0) n = 0;
        if (n > ARROW_COUNT) n = ARROW_COUNT;
        for (int i = 0; i < ARROW_COUNT; i++) {
            if (!set.arrows[i]) continue;
            if (i < n) lv_obj_clear_flag(set.arrows[i], LV_OBJ_FLAG_HIDDEN);
            else       lv_obj_add_flag(set.arrows[i],   LV_OBJ_FLAG_HIDDEN);
        }
    }

    // ============================================================
    //  Master-Pulse: synchrones Pulsieren aller Pfeile
    // ============================================================
    void start_pulse_master() {
        lv_anim_init(&_anim_pulse_master);
        lv_anim_set_var(&_anim_pulse_master, this);
        lv_anim_set_values(&_anim_pulse_master, PULSE_OPA_MIN, PULSE_OPA_MAX);
        lv_anim_set_time(&_anim_pulse_master, PULSE_HALF);
        lv_anim_set_playback_time(&_anim_pulse_master, PULSE_HALF);
        lv_anim_set_repeat_count(&_anim_pulse_master, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&_anim_pulse_master, [](void* var, int32_t v) {
            UiFlow* self = (UiFlow*)var;
            self->_pulse_opa = v;
            self->apply_pulse();
        });
        lv_anim_start(&_anim_pulse_master);
    }

    void apply_pulse() {
        apply_pulse_to_set(_set_pv_hub);
        apply_pulse_to_set(_set_grid_in);
        apply_pulse_to_set(_set_bat_in);
        apply_pulse_to_set(_set_bat_out);
        apply_pulse_to_set(_set_hub_house);
    }

    void apply_pulse_to_set(ArrowSet& set) {
        for (int i = 0; i < ARROW_COUNT; i++) {
            if (!set.arrows[i]) continue;
            lv_obj_set_style_text_opa(set.arrows[i], _pulse_opa, 0);
        }
    }
};