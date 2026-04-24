#pragma once
// ============================================================
//  Setup-/AP-Modus-Screen
//
//  Zeigt an, dass der ESP im AP-Modus ist und wie man sich
//  verbinden kann:
//   - grosser QR-Code (WIFI-Provisioning, Handy scannt & verbindet)
//   - Text-Hinweise: SSID und Setup-URL
//
//  QR-Code-Inhalt: WIFI:T:nopass;S:<ssid>;; — Standard-Format,
//  das von iOS-/Android-Kameras automatisch als WLAN erkannt wird.
// ============================================================
#include <lvgl.h>
#include <extra/libs/qrcode/lv_qrcode.h>
#include <Arduino.h>

#include "ui_theme.h"

class UiSetup {
public:
    void create(lv_obj_t* parent, const char* ap_ssid, const char* url = "192.168.4.1") {
        theme_apply_root(parent);

        // ── Titel ──
        lv_obj_t* title = lv_label_create(parent);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(theme::ACCENT), 0);
        lv_label_set_text(title, "Setup-Modus");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

        // ── Untertitel ──
        lv_obj_t* sub = lv_label_create(parent);
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(sub, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(sub, "Handy-Kamera auf QR halten");
        lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 38);

        // ── QR-Code: WIFI-Provisioning-String ──
        // Format: WIFI:T:<type>;S:<ssid>;P:<pass>;H:<hidden>;;
        // AP ist offen (kein Passwort) → T:nopass
        char qr_text[96];
        snprintf(qr_text, sizeof(qr_text),
                 "WIFI:T:nopass;S:%s;;", ap_ssid);

        lv_obj_t* qr = lv_qrcode_create(parent, 150,
                                        lv_color_hex(0x000000),   // QR-Dunkel (schwarz)
                                        lv_color_hex(0xFFFFFF));  // QR-Hell (weiss, als "Papier")
        lv_qrcode_update(qr, qr_text, strlen(qr_text));
        lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 62);

        // Rahmen um den QR-Code fuer bessere Lesbarkeit (Quiet Zone)
        lv_obj_set_style_bg_color(qr, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(qr, 4, 0);
        lv_obj_set_style_border_color(qr, lv_color_hex(0xFFFFFF), 0);

        // ── Info-Block unten: SSID + URL ──
        lv_obj_t* lbl_ssid_cap = lv_label_create(parent);
        lv_obj_set_style_text_font(lbl_ssid_cap, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_ssid_cap, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(lbl_ssid_cap, "WLAN:");
        lv_obj_align(lbl_ssid_cap, LV_ALIGN_TOP_LEFT, 12, 232);

        lv_obj_t* lbl_ssid = lv_label_create(parent);
        lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_ssid, lv_color_hex(theme::TEXT), 0);
        lv_label_set_text(lbl_ssid, ap_ssid);
        lv_obj_align(lbl_ssid, LV_ALIGN_TOP_LEFT, 12, 250);

        lv_obj_t* lbl_url_cap = lv_label_create(parent);
        lv_obj_set_style_text_font(lbl_url_cap, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_url_cap, lv_color_hex(theme::TEXT_MUTED), 0);
        lv_label_set_text(lbl_url_cap, "Browser:");
        lv_obj_align(lbl_url_cap, LV_ALIGN_TOP_LEFT, 12, 275);

        lv_obj_t* lbl_url = lv_label_create(parent);
        lv_obj_set_style_text_font(lbl_url, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_url, lv_color_hex(theme::GOOD), 0);
        char url_buf[32];
        snprintf(url_buf, sizeof(url_buf), "http://%s", url);
        lv_label_set_text(lbl_url, url_buf);
        lv_obj_align(lbl_url, LV_ALIGN_TOP_LEFT, 12, 293);
    }
};