#include "WiFiManager.h"
#include <WiFi.h>
#include <time.h>
#include "LGFX.h"

extern LGFX tft;

namespace wifi {

static const unsigned long SERIAL_WAIT_MS = 2000;
static const int MAX_WIFI_ATTEMPTS = 10;
static const char* NTP_POOL[] = { "pool.ntp.org", "europe.pool.ntp.org" };
static const char* TZ_STRING = "CET-1CEST,M3.5.0,M10.5.0/3";

void initSerial() {
    Serial.begin(115200);
    const unsigned long start = millis();
    while (!Serial && (millis() - start < SERIAL_WAIT_MS)) {
        delay(10);
    }
}

void connect() {
    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(WIFI_SSID);
    tft.println("Connecting to WiFi...");
    tft.print(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        tft.print(".");
        attempts++;

        if (attempts >= MAX_WIFI_ATTEMPTS) {
            Serial.println("\nFailed to connect to WiFi!");
            tft.fillScreen(TFT_RED);
            tft.setTextColor(TFT_WHITE);
            tft.setTextSize(1.25f);
            tft.setCursor(0, 0);
            tft.println("WiFi Failed!");
            while (1) { delay(100); }
        }
    }

    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("WiFi Connected!");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("IP: ");
    tft.println(WiFi.localIP());
}

void syncTime() {
    Serial.println("Configuring time using NTP...");
    configTzTime(TZ_STRING, NTP_POOL[0], NTP_POOL[1]);
    Serial.print("Waiting for NTP time sync");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println("\nTime synchronized!");

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.print("Current local time: ");
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S (%Z %z)");
    } else {
        Serial.println("Failed to obtain local time");
    }
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool tryReconnect() {
    WiFi.disconnect();
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int i = 0; i < 15; i++) {
        delay(1000);
        if (WiFi.status() == WL_CONNECTED) return true;
    }
    return false;
}

}  // namespace wifi
