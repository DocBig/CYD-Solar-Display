#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// ============================================================
//  Gespeicherte Einstellungen
// ============================================================
struct Settings {
    // WiFi
    char wifi_ssid[64]    = "";
    char wifi_pass[64]    = "";
    // MQTT
    char mqtt_host[64]    = "";
    uint16_t mqtt_port    = 1883;
    char mqtt_user[64]    = "";
    char mqtt_pass[64]    = "";
    char mqtt_prefix[64]  = "SS/deye-12k/";
    // PV
    char pv1_label[16]    = "Sued:";
    char pv2_label[16]    = "West:";
    float pv_max_power    = 15000.0f;
    // Wetter
    char weather_topic[64] = "wetter/koewa";
    // Auto-Rotate
    bool  auto_rotate     = false;
    uint8_t rotate_secs   = 10;
    // Brightness
    uint8_t bright_min    = 15;
    uint8_t bright_max    = 220;
    // Standort
    float latitude        = 51.18f;
    float longitude       = 14.42f;
    // Display
    bool  invert_display  = false;
    char  hostname[32]    = "solar-display";
    // Panel-Variante: "ST7789" (default), "ILI9341", "ILI9342"
    char  panel_type[16]  = "ST7789";

    bool isValid() const {
        return strlen(wifi_ssid) > 0 && strlen(mqtt_host) > 0;
    }
};

// ============================================================
//  WiFi Manager Klasse
// ============================================================
class WifiManager {
public:
    Settings settings;

    // ── NVS laden ──────────────────────────────────────────
    bool loadSettings() {
        Preferences prefs;
        prefs.begin("solar", true);

        if (!prefs.getBool("configured", false)) {
            prefs.end();
            return false;
        }

        _readStr(prefs, "wifi_ssid",    settings.wifi_ssid,    sizeof(settings.wifi_ssid));
        _readStr(prefs, "wifi_pass",    settings.wifi_pass,    sizeof(settings.wifi_pass));
        _readStr(prefs, "mqtt_host",    settings.mqtt_host,    sizeof(settings.mqtt_host));
        settings.mqtt_port = prefs.getUShort("mqtt_port", 1883);
        _readStr(prefs, "mqtt_user",    settings.mqtt_user,    sizeof(settings.mqtt_user));
        _readStr(prefs, "mqtt_pass",    settings.mqtt_pass,    sizeof(settings.mqtt_pass));
        _readStr(prefs, "mqtt_prefix",  settings.mqtt_prefix,  sizeof(settings.mqtt_prefix));
        _readStr(prefs, "pv1_label",    settings.pv1_label,    sizeof(settings.pv1_label));
        _readStr(prefs, "pv2_label",    settings.pv2_label,    sizeof(settings.pv2_label));
        settings.pv_max_power = prefs.getFloat("pv_max", 15000.0f);
        _readStr(prefs, "w_topic",      settings.weather_topic, sizeof(settings.weather_topic));
        settings.auto_rotate  = prefs.getBool("auto_rot", false);
        settings.rotate_secs  = prefs.getUChar("rot_secs", 10);
        settings.bright_min   = prefs.getUChar("br_min", 15);
        settings.bright_max   = prefs.getUChar("br_max", 220);
        settings.latitude     = prefs.getFloat("lat", 51.18f);
        settings.longitude    = prefs.getFloat("lon", 14.42f);
        settings.invert_display = prefs.getBool("inv_disp", false);
        _readStr(prefs, "hostname",     settings.hostname,     sizeof(settings.hostname));
        _readStr(prefs, "panel_type",   settings.panel_type,   sizeof(settings.panel_type));
        // Falls Feld leer (alte NVS ohne panel_type) → Default ST7789
        if (strlen(settings.panel_type) == 0) {
            strncpy(settings.panel_type, "ST7789", sizeof(settings.panel_type) - 1);
        }

        prefs.end();
        return settings.isValid();
    }

    // ── NVS speichern ──────────────────────────────────────
    void saveSettings() {
        Preferences prefs;
        prefs.begin("solar", false);

        prefs.putString("wifi_ssid",   settings.wifi_ssid);
        prefs.putString("wifi_pass",   settings.wifi_pass);
        prefs.putString("mqtt_host",   settings.mqtt_host);
        prefs.putUShort("mqtt_port",   settings.mqtt_port);
        prefs.putString("mqtt_user",   settings.mqtt_user);
        prefs.putString("mqtt_pass",   settings.mqtt_pass);
        prefs.putString("mqtt_prefix", settings.mqtt_prefix);
        prefs.putString("pv1_label",   settings.pv1_label);
        prefs.putString("pv2_label",   settings.pv2_label);
        prefs.putFloat("pv_max",       settings.pv_max_power);
        prefs.putString("w_topic",     settings.weather_topic);
        prefs.putBool("auto_rot",      settings.auto_rotate);
        prefs.putUChar("rot_secs",     settings.rotate_secs);
        prefs.putUChar("br_min",       settings.bright_min);
        prefs.putUChar("br_max",       settings.bright_max);
        prefs.putFloat("lat",          settings.latitude);
        prefs.putFloat("lon",          settings.longitude);
        prefs.putBool("inv_disp",      settings.invert_display);
        prefs.putString("hostname",    settings.hostname);
        prefs.putString("panel_type",  settings.panel_type);
        prefs.putBool("configured",    true);

        prefs.end();
    }

    // ── Factory Reset ──────────────────────────────────────
    void resetSettings() {
        Preferences prefs;
        prefs.begin("solar", false);
        prefs.clear();
        prefs.end();
    }

    // ── AP-Modus starten ───────────────────────────────────
    void startAP(const char* apName = "SolarDisplay-Setup") {
        _apActive = true;

        // AP+STA Modus: AP für Portal, STA für WiFi-Scan
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(apName);
        delay(200);  // AP braucht kurz

        _apIP = WiFi.softAPIP();
        Serial.printf("AP gestartet: %s\n", apName);
        Serial.printf("AP IP: %s\n", _apIP.toString().c_str());

        // DNS: ALLE Anfragen auf AP-IP umleiten → Captive Portal
        _dns.setErrorReplyCode(DNSReplyCode::NoError);
        _dns.start(53, "*", _apIP);

        // Captive Portal Detection Endpoints (Android, iOS, Windows)
        _server.on("/generate_204",    [this]() { _redirectToRoot(); });  // Android
        _server.on("/gen_204",         [this]() { _redirectToRoot(); });  // Android alt
        _server.on("/hotspot-detect.html", [this]() { _redirectToRoot(); });  // Apple
        _server.on("/library/test/success.html", [this]() { _redirectToRoot(); });  // Apple
        _server.on("/ncsi.txt",        [this]() { _redirectToRoot(); });  // Windows
        _server.on("/connecttest.txt", [this]() { _redirectToRoot(); });  // Windows
        _server.on("/fwlink",          [this]() { _redirectToRoot(); });  // Windows

        // Eigene Routen
        _server.on("/",        HTTP_GET,  [this]() { _handleRoot(); });
        _server.on("/save",    HTTP_POST, [this]() { _handleSave(); });
        _server.on("/reset",   HTTP_GET,  [this]() { _handleReset(); });
        _server.on("/scan",    HTTP_GET,  [this]() { _handleScan(); });

        // Alles andere → Redirect auf Root (Captive Portal)
        _server.onNotFound([this]() { _redirectToRoot(); });

        _server.begin();
        Serial.println("Webserver gestartet");
    }

    // ── Loop ───────────────────────────────────────────────
    void loop() {
        if (!_apActive) return;
        _dns.processNextRequest();
        _server.handleClient();
    }

    bool isAPActive() const { return _apActive; }
    bool shouldRestart() const { return _restart; }

private:
    WebServer   _server{80};
    DNSServer   _dns;
    IPAddress   _apIP;
    bool        _apActive = false;
    bool        _restart  = false;

    void _readStr(Preferences& p, const char* key, char* dst, size_t maxLen) {
        String val = p.getString(key, "");
        if (val.length() > 0) {
            strncpy(dst, val.c_str(), maxLen - 1);
            dst[maxLen - 1] = '\0';
        }
        // Wenn leer → Default aus Struct bleibt erhalten
    }

    // ── Redirect → Captive Portal ─────────────────────────
    void _redirectToRoot() {
        String url = "http://" + _apIP.toString() + "/";
        _server.sendHeader("Location", url, true);
        _server.send(302, "text/plain", "");
    }

    // ── Hauptseite ─────────────────────────────────────────
    void _handleRoot() {
        String html = R"rawhtml(
<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Solar Display Setup</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:-apple-system,system-ui,sans-serif;background:#111;color:#eee;padding:16px;max-width:420px;margin:0 auto}
  h1{color:#00e070;font-size:22px;margin-bottom:4px}
  h3{color:#888;font-size:13px;margin-bottom:16px;font-weight:normal}
  .card{background:#1a1a1a;border:1px solid #333;border-radius:12px;padding:16px;margin-bottom:12px}
  .card h2{color:#00aaff;font-size:15px;margin-bottom:10px;border-bottom:1px solid #333;padding-bottom:6px}
  label{display:block;color:#aaa;font-size:12px;margin:8px 0 2px;text-transform:uppercase;letter-spacing:0.5px}
  input[type=text],input[type=password],input[type=number],select{width:100%;padding:10px;background:#222;border:1px solid #444;border-radius:8px;color:#fff;font-size:15px;outline:none}
  input:focus{border-color:#00e070}
  input[type=range]{width:100%;margin:6px 0;accent-color:#00e070}
  .cb{display:flex;align-items:center;gap:8px;margin:8px 0}
  .cb input{width:auto}
  .cb label{margin:0;font-size:14px}
  .row{display:flex;gap:10px}
  .row>*{flex:1}
  button{width:100%;padding:14px;border:none;border-radius:10px;font-size:16px;font-weight:600;cursor:pointer;margin-top:8px;color:#000}
  .btn-save{background:#00e070;color:#000;font-size:18px;padding:16px;margin-top:16px}
  .btn-save:active{background:#00b058}
  .btn-scan{background:#333;color:#00aaff;margin-bottom:8px}
  .btn-reset{background:#222;color:#ff4444;font-size:13px;margin-top:16px;border:1px solid #333}
  #nets{margin:6px 0;min-height:8px}
  .net{display:inline-block;background:#222;border:1px solid #555;border-radius:6px;padding:6px 12px;margin:3px;font-size:14px;cursor:pointer;color:#00e070;font-weight:500}
  .net:hover{background:#333;border-color:#00e070}
  .hint{color:#666;font-size:11px;margin-top:4px}
  .val{color:#00e070;font-size:13px;float:right}
</style>
</head><body>
<h1>&#9889; Solar Display</h1>
<h3>Konfiguration</h3>
<form method="POST" action="/save">

<div class="card">
  <h2>&#128246; WiFi</h2>
  <button type="button" class="btn-scan" onclick="scan()">Netzwerke suchen</button>
  <div id="nets"></div>
  <label>SSID</label>
  <input type="text" name="wifi_ssid" id="ssid" value=")rawhtml" + String(settings.wifi_ssid) + R"rawhtml(">
  <label>Passwort</label>
  <input type="password" name="wifi_pass" value=")rawhtml" + String(settings.wifi_pass) + R"rawhtml(">
</div>

<div class="card">
  <h2>&#128233; MQTT</h2>
  <div class="row">
    <div><label>Host / IP</label>
    <input type="text" name="mqtt_host" value=")rawhtml" + String(settings.mqtt_host) + R"rawhtml("></div>
    <div style="max-width:90px"><label>Port</label>
    <input type="number" name="mqtt_port" value=")rawhtml" + String(settings.mqtt_port) + R"rawhtml("></div>
  </div>
  <label>Benutzer</label>
  <input type="text" name="mqtt_user" value=")rawhtml" + String(settings.mqtt_user) + R"rawhtml(">
  <label>Passwort</label>
  <input type="password" name="mqtt_pass" value=")rawhtml" + String(settings.mqtt_pass) + R"rawhtml(">
  <label>Topic Prefix</label>
  <input type="text" name="mqtt_prefix" value=")rawhtml" + String(settings.mqtt_prefix) + R"rawhtml(">
  <p class="hint">z.B. SS/deye-12k/ oder SUNSYNK/INV1/</p>
</div>

<div class="card">
  <h2>&#9728; PV Konfiguration</h2>
  <div class="row">
    <div><label>String 1 Label</label>
    <input type="text" name="pv1_label" value=")rawhtml" + String(settings.pv1_label) + R"rawhtml("></div>
    <div><label>String 2 Label</label>
    <input type="text" name="pv2_label" value=")rawhtml" + String(settings.pv2_label) + R"rawhtml("></div>
  </div>
  <label>Max PV Leistung (W)</label>
  <input type="number" name="pv_max" value=")rawhtml" + String((int)settings.pv_max_power) + R"rawhtml(">
  <p class="hint">Fuer Gauge-Skalierung (z.B. 15000)</p>
</div>

<div class="card">
  <h2>&#127780; Wetter</h2>
  <label>MQTT Topic</label>
  <input type="text" name="w_topic" value=")rawhtml" + String(settings.weather_topic) + R"rawhtml(">
  <p class="hint">JSON mit temperature, humidity, pressure, wind_speed, wind_bearing, condition</p>
</div>

<div class="card">
  <h2>&#127759; Standort (Sonnenauf-/untergang)</h2>
  <div class="row">
    <div><label>Breitengrad</label>
    <input type="text" name="lat" value=")rawhtml" + String(settings.latitude, 4) + R"rawhtml("></div>
    <div><label>Laengengrad</label>
    <input type="text" name="lon" value=")rawhtml" + String(settings.longitude, 4) + R"rawhtml("></div>
  </div>
  <p class="hint">Fuer automatischen Tag/Nacht-Wechsel der Helligkeit. Helligkeit wird am Display eingestellt.</p>
  <div class="cb">
    <input type="checkbox" name="inv_disp" id="inv_disp" value="1" )rawhtml" + String(settings.invert_display ? "checked" : "") + R"rawhtml(>
    <label for="inv_disp">Display Farben invertieren</label>
  </div>
  <label>Hostname (mDNS)</label>
  <input type="text" name="hostname" value=")rawhtml" + String(settings.hostname) + R"rawhtml(">
  <p class="hint">Erreichbar unter hostname.local (z.B. solar-display.local)</p>

  <label>Display-Controller</label>
  <select name="panel_type">
    <option value="ST7789")rawhtml"  + String(strcmp(settings.panel_type, "ST7789")  == 0 ? " selected" : "") + R"rawhtml(>ST7789 (neuere CYDs mit Type-C)</option>
    <option value="ILI9341")rawhtml" + String(strcmp(settings.panel_type, "ILI9341") == 0 ? " selected" : "") + R"rawhtml(>ILI9341 (klassische CYDs nur Micro-USB)</option>
    <option value="ILI9342")rawhtml" + String(strcmp(settings.panel_type, "ILI9342") == 0 ? " selected" : "") + R"rawhtml(>ILI9342 (neue AliExpress-Charge 2024+)</option>
  </select>
  <p class="hint">Bei falschen Farben, gespiegelter Darstellung oder Rauschen auf dem Display einen anderen Typ waehlen. Standard: ST7789.</p>
</div>

<button type="submit" class="btn-save">&#128190; Speichern &amp; Neustarten</button>
</form>

<button class="btn-reset" onclick="if(confirm('Alle Einstellungen loeschen?'))location='/reset'">Factory Reset</button>

<script>
function scan(){
  document.getElementById('nets').innerHTML='<span style="color:#888">Suche...</span>';
  fetch('/scan').then(r=>r.json()).then(d=>{
    d.sort((a,b)=>b.r-a.r);
    let h='';
    d.forEach(n=>{
      let sig=n.r>-60?'\u25CF\u25CF\u25CF':n.r>-75?'\u25CF\u25CF':'\u25CF';
      let name=n.s.replace(/'/g,"\\'");
      h+='<span class="net" onclick="document.getElementById(\'ssid\').value=\''+name+'\'">'+n.s+' <small style="color:#888">'+sig+'</small></span> ';
    });
    document.getElementById('nets').innerHTML=h||'<span style="color:#f44">Keine Netzwerke gefunden</span>';
  }).catch(()=>{document.getElementById('nets').innerHTML='<span style="color:#f44">Scan fehlgeschlagen</span>';});
}
</script>
</body></html>
)rawhtml";

        _server.send(200, "text/html", html);
    }

    // ── Speichern ──────────────────────────────────────────
    void _handleSave() {
        auto arg = [&](const char* name) -> String { return _server.arg(name); };

        strncpy(settings.wifi_ssid,   arg("wifi_ssid").c_str(),   sizeof(settings.wifi_ssid) - 1);
        strncpy(settings.wifi_pass,   arg("wifi_pass").c_str(),   sizeof(settings.wifi_pass) - 1);
        strncpy(settings.mqtt_host,   arg("mqtt_host").c_str(),   sizeof(settings.mqtt_host) - 1);
        settings.mqtt_port = arg("mqtt_port").toInt();
        if (settings.mqtt_port == 0) settings.mqtt_port = 1883;
        strncpy(settings.mqtt_user,   arg("mqtt_user").c_str(),   sizeof(settings.mqtt_user) - 1);
        strncpy(settings.mqtt_pass,   arg("mqtt_pass").c_str(),   sizeof(settings.mqtt_pass) - 1);
        strncpy(settings.mqtt_prefix, arg("mqtt_prefix").c_str(), sizeof(settings.mqtt_prefix) - 1);
        strncpy(settings.pv1_label,   arg("pv1_label").c_str(),   sizeof(settings.pv1_label) - 1);
        strncpy(settings.pv2_label,   arg("pv2_label").c_str(),   sizeof(settings.pv2_label) - 1);
        settings.pv_max_power = arg("pv_max").toFloat();
        if (settings.pv_max_power < 100) settings.pv_max_power = 15000;

        // Wetter
        strncpy(settings.weather_topic, arg("w_topic").c_str(), sizeof(settings.weather_topic) - 1);

        // Standort
        settings.latitude  = arg("lat").toFloat();
        settings.longitude = arg("lon").toFloat();
        if (settings.latitude == 0 && settings.longitude == 0) {
            settings.latitude = 51.18f;
            settings.longitude = 14.42f;
        }

        // Display
        settings.invert_display = _server.hasArg("inv_disp");
        strncpy(settings.hostname, arg("hostname").c_str(), sizeof(settings.hostname) - 1);
        if (strlen(settings.hostname) == 0) strcpy(settings.hostname, "solar-display");
        strncpy(settings.panel_type, arg("panel_type").c_str(), sizeof(settings.panel_type) - 1);
        // Validierung: nur die drei bekannten Werte akzeptieren, sonst Default
        if (strcmp(settings.panel_type, "ST7789") != 0 &&
            strcmp(settings.panel_type, "ILI9341") != 0 &&
            strcmp(settings.panel_type, "ILI9342") != 0) {
            strcpy(settings.panel_type, "ST7789");
        }

        saveSettings();

        String html = R"rawhtml(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:sans-serif;background:#111;color:#eee;display:flex;align-items:center;justify-content:center;height:100vh;flex-direction:column}
h1{color:#00e070;font-size:28px}p{color:#888;margin-top:10px}</style>
</head><body>
<h1>&#10004; Gespeichert!</h1>
<p>Neustart in 3 Sekunden...</p>
</body></html>
)rawhtml";

        _server.send(200, "text/html", html);
        _restart = true;
    }

    // ── Factory Reset ──────────────────────────────────────
    void _handleReset() {
        resetSettings();

        String html = R"rawhtml(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<style>body{font-family:sans-serif;background:#111;color:#eee;display:flex;align-items:center;justify-content:center;height:100vh;flex-direction:column}
h1{color:#ff4444;font-size:28px}p{color:#888;margin-top:10px}</style>
</head><body>
<h1>&#128465; Reset!</h1>
<p>Neustart in 3 Sekunden...</p>
</body></html>
)rawhtml";

        _server.send(200, "text/html", html);
        _restart = true;
    }

    // ── WiFi Scan ──────────────────────────────────────────
    void _handleScan() {
        int n = WiFi.scanNetworks(false, false, false, 300);
        String json = "[";
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            // SSID escapen für JSON
            String ssid = WiFi.SSID(i);
            ssid.replace("\\", "\\\\");
            ssid.replace("\"", "\\\"");
            json += "{\"s\":\"" + ssid + "\",\"r\":" + String(WiFi.RSSI(i)) + "}";
        }
        json += "]";
        WiFi.scanDelete();

        _server.send(200, "application/json", json);
    }
};
