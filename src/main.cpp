// ============================================================
//  Solar-Display — LVGL + LovyanGFX + MQTT
//  ESP32-2432S028 (Cheap Yellow Display, ST7789-Variante)
//
//  Features:
//    - Tileview mit drei horizontal wischbaren Seiten:
//        * Solar  — SOC-Arc, PV-Leistung, Netz/Last, Tagesertrag
//        * Wetter — Bitmap-Icon, Temperatur, Feuchte, Wind, Druck
//        * Flow   — Energiefluss-Diagramm (PV/Akku/Netz/Haus)
//    - Settings-Seite vertikal unter Solar
//    - Auto-Rotate (optional, aus Settings) — rotiert horizontal
//    - Day/Night-Brightness via SunCalc + sanfter Daemmerungsblende
//    - MQTT-Daten live: Solar-Inverter + Wetter (HA-Format)
//    - Setup-Modus mit QR-Code-gestuetztem AP
//      (BOOT-Taste 3s halten oder leere Settings)
//
//  UI-Code: src/ui/ui_solar.h, ui_weather.h, ui_flow.h, ui_setup.h
//  Theme:   src/ui/ui_theme.h (Farbpalette + Hilfen)
//  Daten:   solar_data.h, weather_data.h
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>

#include "lgfx_cyd.h"
#include "solar_data.h"
#include "weather_data.h"
#include "wifi_manager.h"
#include "sun_calc.h"
#include "brightness.h"
#include "ui/ui_solar.h"
#include "ui/ui_weather.h"
#include "ui/ui_flow.h"
#include "ui/ui_setup.h"
#include "ui/ui_settings.h"

// ============================================================
//  Globale Objekte
// ============================================================
// lcd wird in setup() nach dem Laden der Settings via
// create_lcd_from_panel_type() erzeugt, damit der Panel-Typ
// (ST7789 / ILI9341 / ILI9342) zur Laufzeit konfigurierbar ist.
static lgfx::LGFX_Device* lcd = nullptr;
static SolarData    solar;
static WeatherData  weather;
static WifiManager  wifiMgr;
static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);
static UiSolar      ui_solar_page;
static UiWeather    ui_weather_page;
static UiFlow       ui_flow_page;
static UiSetup      ui_setup_page;
static UiSettings   ui_settings_page;
static SunCalc      sunCalc;
static Brightness   brightness;

// Handles fuers Auto-Rotate (werden in setup() gefuellt, sofern Normal-Modus)
static lv_obj_t*    tileview     = nullptr;
static uint32_t     tile_ids_max = 0;

// ── LVGL Draw-Buffer ───────────────────────────────────────
// Nur EIN Buffer (nicht zwei), damit genug DRAM für WiFi/WebServer
// bleibt. 30 Zeilen × 240 × 2 Byte = 14,4 KB. Bei Bedarf auf 25
// runter, wenn's nochmal eng wird.
static constexpr int SCREEN_W  = 240;
static constexpr int SCREEN_H  = 320;
static constexpr int BUF_LINES = 30;
static lv_color_t    buf1[SCREEN_W * BUF_LINES];
static lv_color_t    buf2[SCREEN_W * BUF_LINES];   // ← NEU
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t      disp_drv;
static lv_indev_drv_t     indev_drv;

// ── Timing ─────────────────────────────────────────────────
static unsigned long t_clock_prev   = 0;
static unsigned long t_wifi_prev    = 0;
static unsigned long t_lvgl_prev    = 0;
static unsigned long t_reconnect    = 0;
static unsigned long t_buttonStart  = 0;
static unsigned long t_last_swipe   = 0;   // letzte Benutzer-Interaktion
static unsigned long t_bright_prev  = 0;   // letzte Helligkeits-Aktualisierung
static unsigned long t_sun_prev     = 0;   // letzte SunCalc-Neuberechnung

// ── AP-Trigger / Boot-Konfig ───────────────────────────────
#define AP_TRIGGER_PIN  0
#define AP_NAME         "SolarDisplay-Setup"
#define NTP_SERVER      "pool.ntp.org"
#define TZ_INFO         "CET-1CEST,M3.5.0,M10.5.0/3"
static bool inAPMode = false;

// ============================================================
//  Tileview-Event: User hat gewischt → Auto-Rotate zuruecksetzen
// ============================================================
static void on_tile_changed(lv_event_t*) {
    t_last_swipe = millis();
}

// ============================================================
//  LVGL ↔ LovyanGFX Bridge
// ============================================================
static void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    lcd->startWrite();
    lcd->setAddrWindow(area->x1, area->y1, w, h);
    lcd->pushPixelsDMA((uint16_t*)color_p, w * h);
    lcd->endWrite();
    lv_disp_flush_ready(drv);
}

static void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint16_t x, y;
    if (lcd->getTouch(&x, &y)) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ============================================================
//  MQTT Callback
// ============================================================
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char value[512];
    int len = min((unsigned int)511, length);
    memcpy(value, payload, len);
    value[len] = '\0';

    // ── Wetter-JSON ──
    const char* wTopic = wifiMgr.settings.weather_topic;
    if (strlen(wTopic) > 0 && strcmp(topic, wTopic) == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, value)) return;
        weather.temperature  = doc["temperature"]  | NAN;
        weather.humidity     = doc["humidity"]     | NAN;
        weather.pressure     = doc["pressure"]     | NAN;
        weather.wind_speed   = doc["wind_speed"]   | NAN;
        weather.wind_bearing = doc["wind_bearing"] | NAN;
        const char* cond = doc["condition"] | "unknown";
        strncpy(weather.condition, cond, sizeof(weather.condition) - 1);
        weather.last_update = millis();
        return;
    }

    // ── Solar-Werte ──
    float val = atof(value);
    const char* prefix = wifiMgr.settings.mqtt_prefix;
    if (strncmp(topic, prefix, strlen(prefix)) != 0) return;
    const char* suffix = topic + strlen(prefix);

    if (strcmp(suffix, "grid_connected") == 0) {
        solar.grid_connected = (val > 0 || strcmp(value, "ON") == 0);
        solar.last_update = millis();
        return;
    }
    for (int i = 0; i < TOPIC_COUNT; i++) {
        if (strcmp(suffix, TOPIC_MAPS[i].suffix) == 0) {
            solar.*(TOPIC_MAPS[i].field) = val;
            solar.last_update = millis();
            return;
        }
    }
}

// ============================================================
//  WiFi verbinden
// ============================================================
static bool wifiConnect() {
    const Settings& s = wifiMgr.settings;
    Serial.printf("[WiFi] Verbinde mit %s ...\n", s.wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(s.wifi_ssid, s.wifi_pass);

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 30) {
        delay(500);
        Serial.print(".");
        tries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] OK! IP: %s\n", WiFi.localIP().toString().c_str());
        configTzTime(TZ_INFO, NTP_SERVER);
        if (strlen(s.hostname) > 0) {
            WiFi.setHostname(s.hostname);
            if (MDNS.begin(s.hostname))
                Serial.printf("[mDNS] %s.local\n", s.hostname);
        }
        return true;
    }
    Serial.println("\n[WiFi] Fehlgeschlagen!");
    return false;
}

// ============================================================
//  MQTT verbinden + Subscribes
// ============================================================
static void mqttConnect() {
    const Settings& s = wifiMgr.settings;
    mqtt.setServer(s.mqtt_host, s.mqtt_port);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(1024);

    Serial.printf("[MQTT] Verbinde mit %s:%d ...\n", s.mqtt_host, s.mqtt_port);
    const char* clientId = strlen(s.hostname) > 0 ? s.hostname : "solar-display";

    if (mqtt.connect(clientId, s.mqtt_user, s.mqtt_pass)) {
        Serial.println("[MQTT] Verbunden!");

        // Solar-Topics subscriben
        for (int i = 0; i < TOPIC_COUNT; i++) {
            String topic = String(s.mqtt_prefix) + TOPIC_MAPS[i].suffix;
            mqtt.subscribe(topic.c_str());
        }
        mqtt.subscribe((String(s.mqtt_prefix) + "grid_connected").c_str());

        // Wetter
        if (strlen(s.weather_topic) > 0) {
            mqtt.subscribe(s.weather_topic);
            Serial.printf("[MQTT] Wetter-Topic: %s\n", s.weather_topic);
        }
    } else {
        Serial.printf("[MQTT] Fehler (rc=%d)\n", mqtt.state());
    }
}

// ============================================================
//  Verbindungs-Watchdog
// ============================================================
static void checkConnections() {
    if (millis() - t_reconnect < 5000) return;
    t_reconnect = millis();
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) mqttConnect();
}

// ============================================================
//  BOOT-Taste → AP-Modus
// ============================================================
static void checkAPTrigger() {
    if (digitalRead(AP_TRIGGER_PIN) == LOW) {
        if (t_buttonStart == 0) {
            t_buttonStart = millis();
        } else if (millis() - t_buttonStart > 3000) {
            Preferences prefs;
            prefs.begin("solar", false);
            prefs.putBool("force_ap", true);
            prefs.end();
            Serial.println("[Boot] AP-Modus erzwungen, Neustart …");
            delay(100);
            ESP.restart();
        }
    } else {
        t_buttonStart = 0;
    }
}

// ============================================================
//  Callbacks fuer die Settings-Seite
// ============================================================
static void settings_save() {
    wifiMgr.saveSettings();
}

static void settings_restart_to_ap() {
    Serial.println("[Settings] Neustart in AP-Modus...");
    Preferences prefs;
    prefs.begin("solar", false);
    prefs.putBool("force_ap", true);
    prefs.end();
    delay(100);
    ESP.restart();
}

static void settings_bright_preview(uint8_t value, bool /*is_day*/) {
    // Live-Vorschau: direkt auf Display anwenden
    lcd->setBrightness(value);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n[Boot] Solar Display (LVGL)");

    pinMode(AP_TRIGGER_PIN, INPUT_PULLUP);

    // ── Settings FRUEH laden, damit wir den richtigen Panel-Typ
    //    kennen bevor wir lcd->begin() aufrufen ──
    bool haveSettings = wifiMgr.loadSettings();

    // ── LCD-Instanz passend zum konfigurierten Panel-Typ erzeugen ──
    // Default ist ST7789 (falls Settings leer oder Panel-Typ unbekannt).
    const char* panel_type = haveSettings ? wifiMgr.settings.panel_type
                                          : "ST7789";
    lcd = create_lcd_from_panel_type(panel_type);
    Serial.printf("[Boot] Panel-Typ: %s\n", panel_type);

    // ── Display (erstmal Standard-Helligkeit, wird spaeter
    //    via Brightness-Controller angepasst) ──
    lcd->begin();
    // Panel-spezifische Rotation: ILI9341/9342-Varianten sind im
    // CYD um 180 Grad verdreht eingebaut und muessen software-
    // seitig gedreht werden. setRotation() dreht auch den Touch
    // mit, daher wird das hier gemacht und nicht im Panel-Config.
    if (strcmp(panel_type, "ILI9341") == 0 ||
        strcmp(panel_type, "ILI9342") == 0) {
        lcd->setRotation(2);   // 180 Grad gedreht
    } else {
        lcd->setRotation(0);
    }
    lcd->setBrightness(200);
    lcd->fillScreen(TFT_BLACK);

    // ── LVGL ──
    lv_init();
    //lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, SCREEN_W * BUF_LINES);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_W * BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCREEN_W;
    disp_drv.ver_res  = SCREEN_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    // ── force_ap Flag prüfen (von BOOT-Taste) ──
    {
        Preferences prefs;
        prefs.begin("solar", false);
        bool force = prefs.getBool("force_ap", false);
        if (force) {
            prefs.putBool("force_ap", false);
            prefs.end();
            inAPMode = true;
        } else {
            prefs.end();
        }
    }

    // ── WiFi/AP-Entscheidung treffen ──
    if (!haveSettings || inAPMode) {
        Serial.println("[Boot] → AP-Modus (Setup)");
        inAPMode = true;
        wifiMgr.startAP(AP_NAME);
    } else {
        if (!wifiConnect()) {
            Serial.println("[Boot] WiFi fehlgeschlagen → AP-Modus");
            inAPMode = true;
            wifiMgr.startAP(AP_NAME);
        } else {
            mqttConnect();
            // SunCalc mit Standort aus Settings fuettern + erste Berechnung
            sunCalc.latitude  = wifiMgr.settings.latitude;
            sunCalc.longitude = wifiMgr.settings.longitude;
            // Kurz warten, damit NTP die Zeit setzen kann
            delay(300);
            sunCalc.update();
            Serial.printf("[Sun] Sonnenaufgang: %.2fh, Untergang: %.2fh\n",
                          sunCalc.sunrise, sunCalc.sunset);
            // Direkt die berechnete Helligkeit setzen, damit's nicht
            // erst im Loop nach 30s korrigiert wird.
            lcd->setBrightness(brightness.compute(sunCalc,
                wifiMgr.settings.bright_min,
                wifiMgr.settings.bright_max));
        }
    }

    // ── UI aufbauen: entweder Setup-Screen oder Tileview ──
    if (inAPMode) {
        // AP-Modus: dedizierte Setup-Seite mit QR und Text
        ui_setup_page.create(lv_scr_act(), AP_NAME, "192.168.4.1");
    } else {
        // Normal-Modus: Tileview mit vier Tiles in 2D-Layout
        //
        //   (0,0) Flow   <->  (1,0) Solar  <->  (2,0) Wetter    ← horizontal
        //                            ↕
        //                      (1,1) Settings                   ← vertikal (von Solar)
        //
        // Auto-Rotate rotiert nur in der horizontalen Reihe.
        lv_obj_t* tv = lv_tileview_create(lv_scr_act());
        lv_obj_set_style_bg_color(tv, lv_color_hex(0x0E1116), 0);
        lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* tile_flow     = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
        lv_obj_t* tile_solar    = lv_tileview_add_tile(tv, 1, 0,
                                    (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT | LV_DIR_BOTTOM));
        lv_obj_t* tile_weather  = lv_tileview_add_tile(tv, 2, 0, LV_DIR_LEFT);
        lv_obj_t* tile_settings = lv_tileview_add_tile(tv, 1, 1, LV_DIR_TOP);

        ui_flow_page.create(tile_flow);
        ui_solar_page.create(tile_solar);
        ui_solar_page.set_pv_labels(wifiMgr.settings.pv1_label,
                                    wifiMgr.settings.pv2_label);
        ui_solar_page.set_daily_goal(wifiMgr.settings.daily_pv_goal_kwh);
        ui_weather_page.create(tile_weather);

        // Settings-Seite mit Callbacks verdrahten
        ui_settings_page.on_save_settings  = settings_save;
        ui_settings_page.on_restart_to_ap  = settings_restart_to_ap;
        ui_settings_page.on_bright_preview = settings_bright_preview;
        ui_settings_page.create(tile_settings, &wifiMgr.settings);

        // User-Wisch-Detection: bei Tile-Wechsel Rotate-Timer zuruecksetzen
        lv_obj_add_event_cb(tv, on_tile_changed, LV_EVENT_VALUE_CHANGED, nullptr);
        t_last_swipe = millis();

        // Default auf Solar setzen
        lv_obj_set_tile_id(tv, 1, 0, LV_ANIM_OFF);

        // Handles merken für Auto-Rotate (horizontal: 3 Tiles)
        tileview     = tv;
        tile_ids_max = 3;
    }

    Serial.printf("[Boot] RAM frei: %u Bytes\n", ESP.getFreeHeap());
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    unsigned long now = millis();

    // ── LVGL-Tick füttern ──
    lv_tick_inc(now - t_lvgl_prev);
    t_lvgl_prev = now;

    // ── AP-Modus: Webserver bedienen, Rest überspringen ──
    if (inAPMode) {
        wifiMgr.loop();
        lv_timer_handler();
        delay(5);
        return;
    }

    // ── BOOT-Taste prüfen ──
    checkAPTrigger();

    // ── Uhrzeit einmal pro 10 Sekunden (alle Seiten) ──
    if (now - t_clock_prev > 10000) {
        t_clock_prev = now;
        ui_solar_page.update_clock();
        ui_weather_page.update_clock();
        ui_flow_page.update_clock();
    }

    // ── WiFi-Indikator alle 10s (alle Seiten) ──
    if (now - t_wifi_prev > 10000) {
        t_wifi_prev = now;
        bool online = (WiFi.status() == WL_CONNECTED) && mqtt.connected();
        int  rssi   = WiFi.RSSI();
        ui_solar_page.update_wifi(rssi, online);
        ui_weather_page.update_wifi(rssi, online);
        ui_flow_page.update_wifi(rssi, online);
    }

    // ── MQTT bedienen ──
    mqtt.loop();
    checkConnections();

    // ── UI-Update bei neuen Daten ──
    static unsigned long last_solar_render   = 0;
    static unsigned long last_weather_render = 0;
    if (solar.last_update != last_solar_render) {
        last_solar_render = solar.last_update;
        ui_solar_page.update(solar);
        ui_flow_page.update(solar);
    }
    if (weather.last_update != last_weather_render) {
        last_weather_render = weather.last_update;
        ui_weather_page.update(weather);
    }

    // ── Auto-Rotate ──
    // Nur aktiv, wenn in Settings aktiviert und Tileview existiert.
    // Rotiert nur in der horizontalen Reihe (row 0) zwischen Solar,
    // Wetter und Flow. Wenn die Settings-Seite aktiv ist (row 1),
    // wird nicht rotiert, damit der User in Ruhe einstellen kann.
    if (tileview && wifiMgr.settings.auto_rotate && tile_ids_max > 1) {
        uint32_t rotate_ms = (uint32_t)wifiMgr.settings.rotate_secs * 1000;
        if (rotate_ms < 3000) rotate_ms = 3000;   // Sicherheitsuntergrenze
        if (now - t_last_swipe > rotate_ms) {
            // Aktuelle Tile-ID (col,row) aus der Position ableiten
            lv_obj_t* act = lv_tileview_get_tile_act(tileview);
            uint32_t cur_col = 0, cur_row = 0;
            if (act) {
                lv_coord_t w = lv_obj_get_width(tileview);
                lv_coord_t h = lv_obj_get_height(tileview);
                if (w > 0) cur_col = lv_obj_get_x(act) / w;
                if (h > 0) cur_row = lv_obj_get_y(act) / h;
            }
            // Nur in der horizontalen Reihe rotieren
            if (cur_row == 0) {
                uint32_t next_col = (cur_col + 1) % tile_ids_max;
                lv_obj_set_tile_id(tileview, next_col, 0, LV_ANIM_ON);
                t_last_swipe = now;
            }
        }
    }

    // ── SunCalc 1x pro Stunde neu berechnen ──
    if (now - t_sun_prev > 3600000UL || t_sun_prev == 0) {
        t_sun_prev = now;
        sunCalc.update();
    }

    // ── Display-Helligkeit anpassen (alle 30s reicht) ──
    // Nicht ueberschreiben, solange der Settings-Slider live
    // eine Vorschau anzeigt (3s nach letzter Beruehrung aus).
    if ((now - t_bright_prev > 30000UL || t_bright_prev == 0)
        && !ui_settings_page.preview_active()) {
        t_bright_prev = now;
        uint8_t b = brightness.compute(
            sunCalc,
            wifiMgr.settings.bright_min,
            wifiMgr.settings.bright_max);
        lcd->setBrightness(b);
    }

    // ── Settings-Tick 1x pro Sekunde (Info-Panel + Preview-Timer) ──
    static unsigned long t_settings_prev = 0;
    if (now - t_settings_prev > 1000) {
        t_settings_prev = now;
        ui_settings_page.tick();
    }

    // ── LVGL Render-Zyklus ──
    //uint32_t time_till_next = lv_timer_handler();
    //if (time_till_next == LV_NO_TIMER_READY) time_till_next = 5;
    //if (time_till_next > 20) time_till_next = 20;  // Cap, damit der Rest des Loops nicht hängt
    //delay(time_till_next);


    // ── Performance-Diagnose ──
    static unsigned long t_diag = 0;
    static uint32_t loop_count = 0;
    static uint32_t lvgl_time_total = 0;
    static uint32_t lvgl_time_max = 0;
    loop_count++;

    unsigned long t_before = micros();
    uint32_t time_till_next = lv_timer_handler();
    unsigned long t_lvgl = micros() - t_before;
    lvgl_time_total += t_lvgl;
    if (t_lvgl > lvgl_time_max) lvgl_time_max = t_lvgl;

    if (now - t_diag > 1000) {
        Serial.printf("[DIAG] loops=%lu, lvgl_avg=%luµs, lvgl_max=%luµs, heap=%u\n",
                    loop_count,
                    loop_count > 0 ? lvgl_time_total / loop_count : 0,
                    lvgl_time_max,
                    ESP.getFreeHeap());
        loop_count = 0;
        lvgl_time_total = 0;
        lvgl_time_max = 0;
        t_diag = now;
    }

    if (time_till_next == LV_NO_TIMER_READY) time_till_next = 5;
    if (time_till_next > 20) time_till_next = 20;
    delay(time_till_next);



}