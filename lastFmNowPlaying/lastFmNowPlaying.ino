/*
 * Last.fm Now Playing — ESP32 CYD (Cheap Yellow Display)
 * Main sketch: setup and loop only.
 */

#include <time.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include "LGFX.h"
#include "config.h"
#include "apiConfig.h"
#include "userSettings.h"
#include "fetch.h"
#include "display.h"
#include "WiFiManager.h"
#include "LastFmApp.h"

LGFX tft;
// Font must be defined here so U8g2lib.h (included above) is in scope for u8g2_font_unifont_t_extended.
const lgfx::U8g2font myExtendedFont(u8g2_font_unifont_t_extended);

void setup() {
    wifi::initSerial();
    displayInit();
    wifi::connect();
    wifi::syncTime();
    delay(1000);

    Serial.println("Fetching Now Playing data...");
    displayShowFetching();
    lastFmFetchAndDisplay();
}

void loop() {
    if (!wifi::isConnected()) {
        Serial.println("WiFi disconnected. Reconnecting...");
        displayShowWifiReconnecting();
        if (wifi::tryReconnect()) {
            Serial.println("Reconnected.");
        }
        delay(2000);
        return;
    }

    Serial.printf("Free RAM: %u bytes\n", ESP.getFreeHeap());
    lastFmFetchAndDisplay();
    delay(REFRESH_MS);
}
