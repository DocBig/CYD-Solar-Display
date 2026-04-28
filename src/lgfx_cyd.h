#pragma once
// ============================================================
//  LovyanGFX-Konfiguration für ESP32-2432S028 (Cheap Yellow Display)
//
//  Die CYD-Boards existieren in mindestens drei Varianten mit
//  unterschiedlichen TFT-Controllern:
//
//    - ST7789   — Neuere Boards (Type-C USB vorhanden), BGR-Order,
//                 typischerweise invertiert
//    - ILI9341  — Klassische Variante, RGB-Order, nicht invertiert
//    - ILI9342  — Wie ILI9341, aber mit 240x320 Offset-Korrektur
//                 (einige AliExpress-Charges ab 2024)
//
//  Die Panel-Auswahl erfolgt zur Laufzeit aus den Settings.
//  Default ist ST7789 (haeufigste aktuelle Variante).
//
//  Pin-Belegung (bei allen drei Varianten gleich):
//    Panel:   HSPI (SCK=14, MOSI=13, MISO=12, CS=15, DC=2)
//    Touch:   VSPI XPT2046 (SCK=25, MOSI=32, MISO=39, CS=33, IRQ=36)
//    Backlight: GPIO 21 (PWM)
// ============================================================
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <string.h>

// ============================================================
//  Basis-Template: kapselt Bus-, Light- und Touch-Konfig, die
//  bei allen Varianten identisch ist. Das konkrete Panel wird
//  als Template-Parameter eingesetzt.
// ============================================================
template<typename PanelT>
class LGFX_CYD_Variant : public lgfx::LGFX_Device {
protected:
    PanelT               _panel_instance;
    lgfx::Bus_SPI        _bus_instance;
    lgfx::Light_PWM      _light_instance;
    lgfx::Touch_XPT2046  _touch_instance;

public:
    LGFX_CYD_Variant(bool invert_pixel, bool bgr_order,
                     uint_fast8_t rotation_offset = 0,
                     uint_fast16_t touch_offset_rotation = 180,
                     bool invert_touch_x = false,
                     bool invert_touch_y = false) {
        // ── SPI-Bus fuers Panel (HSPI) ──
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host    = HSPI_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 60000000;
            cfg.freq_read   = 20000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 14;
            cfg.pin_mosi    = 13;
            cfg.pin_miso    = 12;
            cfg.pin_dc      = 2;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ── Panel ──
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs          = 15;
            cfg.pin_rst         = -1;
            cfg.pin_busy        = -1;
            cfg.memory_width    = 240;
            cfg.memory_height   = 320;
            cfg.panel_width     = 240;
            cfg.panel_height    = 320;
            cfg.offset_x        = 0;
            cfg.offset_y        = 0;
            cfg.offset_rotation = rotation_offset;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable        = false;
            cfg.invert          = invert_pixel;  // variantenabhaengig
            cfg.rgb_order       = bgr_order;     // variantenabhaengig
            cfg.dlen_16bit      = false;
            cfg.bus_shared      = false;
            _panel_instance.config(cfg);
        }

        // ── Backlight (PWM) ──
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = 21;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        // ── Touch (XPT2046 auf separatem VSPI-Bus) ──
        {
            auto cfg = _touch_instance.config();
            // Default-Kalibrierung (bereits invers, passt zu ST7789)
            cfg.x_min = 3900; cfg.x_max = 300;
            cfg.y_min = 3900; cfg.y_max = 300;
            // Falls eine Achse gespiegelt ist → min/max tauschen
            if (invert_touch_x) { auto t = cfg.x_min; cfg.x_min = cfg.x_max; cfg.x_max = t; }
            if (invert_touch_y) { auto t = cfg.y_min; cfg.y_min = cfg.y_max; cfg.y_max = t; }
            cfg.pin_int    = 36;
            cfg.bus_shared = false;
            cfg.offset_rotation = touch_offset_rotation;
            cfg.spi_host   = VSPI_HOST;
            cfg.freq       = 1000000;
            cfg.pin_sclk   = 25;
            cfg.pin_mosi   = 32;
            cfg.pin_miso   = 39;
            cfg.pin_cs     = 33;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

// ============================================================
//  Die drei konkreten Panel-Varianten
// ============================================================

// ST7789: typ. neuere 2-USB-Variante (Type-C + Micro)
//   - invert=false  (der Panel_ST7789-Treiber invertiert intern bereits)
//   - rgb_order=false  (BGR-Compensation vom Treiber geregelt)
class LGFX_ST7789 : public LGFX_CYD_Variant<lgfx::Panel_ST7789> {
public:
    LGFX_ST7789() : LGFX_CYD_Variant<lgfx::Panel_ST7789>(false, false) {}
};

// ILI9341: klassische CYD-Variante (nur Micro-USB)
// Das Board hat das Panel um 180 Grad verdreht verbaut.
// Kompensation: setRotation(2) in main.cpp (rotiert Bild + Touch)
// PLUS beide Touch-Achsen invertieren, weil die Software-Rotation
// um 180 Grad den Touch nicht ausreichend mitdreht.
// Farb-Order: wie ST7789 BGR (rgb_order=false) — bei rgb_order=true
// erscheinen Rot und Blau vertauscht.
class LGFX_ILI9341 : public LGFX_CYD_Variant<lgfx::Panel_ILI9341> {
public:
    LGFX_ILI9341() : LGFX_CYD_Variant<lgfx::Panel_ILI9341>(
        false, false, 0, 180, true, true) {}
};

// ILI9342: neuere 240x320-Variante aus 2024 AliExpress-Chargen
class LGFX_ILI9342 : public LGFX_CYD_Variant<lgfx::Panel_ILI9342> {
public:
    LGFX_ILI9342() : LGFX_CYD_Variant<lgfx::Panel_ILI9342>(
        true, false, 0, 180, true, true) {}
};

// ============================================================
//  Factory: erzeugt die richtige Instanz aus einem String.
//  Ownership liegt beim Aufrufer (muss delete).
//
//  Unbekannte Strings → ST7789 (Default, am weitesten verbreitet).
// ============================================================
inline lgfx::LGFX_Device* create_lcd_from_panel_type(const char* panel_type) {
    if (panel_type) {
        if (strcmp(panel_type, "ILI9341") == 0) return new LGFX_ILI9341();
        if (strcmp(panel_type, "ILI9342") == 0) return new LGFX_ILI9342();
    }
    return new LGFX_ST7789();  // Default
}

// ============================================================
//  Rueckwaerts-Kompatibilitaet: die bisherige LGFX-Klasse
//  bleibt als Alias fuer ST7789 erhalten, damit bestehender
//  Code ohne Aenderung weiterlaeuft.
// ============================================================
using LGFX = LGFX_ST7789;