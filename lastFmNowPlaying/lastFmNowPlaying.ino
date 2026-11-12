#include <U8g2lib.h>
#include <time.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "LGFX.h"
#include "config.h"
#include "converter.h"
#include "apiConfig.h"
#include "userSettings.h"
#include "fetch.h"
#include "display.h"
#include "WiFiManager.h"
#include "LastFmApp.h"
#include "systemInit.h"

LGFX tft;
// Font must be defined here so U8g2lib.h (included above) is in scope for u8g2_font_unifont_t_extended.
const lgfx::U8g2font myExtendedFont(u8g2_font_unifont_t_extended);

void setup() {
    systemInit::initSerial();
    Serial.println("START");
    display::init();
    wifi::connect();
    systemInit::syncTime();
    delay(1000);

    Serial.println("Fetching Now Playing data...");
    display::showFetching();
    lastFmFetchAndDisplay();
}

void loop() {
    if (!wifi::isConnected()) {
        Serial.println("WiFi disconnected. Reconnecting...");
        display::showWifiReconnecting();
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
