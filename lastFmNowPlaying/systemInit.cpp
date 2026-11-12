#include <Arduino.h>
#include <time.h>
#include "LGFX.h"

extern LGFX tft;

namespace systemInit {

static const unsigned long SERIAL_WAIT_MS = 2000;
static const char* TZ_STRING = "CET-1CEST,M3.5.0,M10.5.0/3";

void initSerial() {
    Serial.begin(115200);
    const unsigned long start = millis();
    while (!Serial && (millis() - start < SERIAL_WAIT_MS)) {
        delay(10);
    }
}

void syncTime() {
    Serial.println("Configuring time using NTP...");

    const char* ntpServers[] = {
        "tempus1.gum.gov.pl",
        "pool.ntp.org",
        "162.159.200.1"
    };

    time_t now = time(nullptr);
    int attempts = 0;
    const int maxAttempts = 5;

    while (now < 8 * 3600 * 2 && attempts < maxAttempts) {
        attempts++;
        const char* currentServer = ntpServers[(attempts - 1) % 3];

        Serial.printf("[Attempt %d/%d] Querying NTP server: %s\n", attempts, maxAttempts, currentServer);
        
        configTzTime(TZ_STRING, currentServer);

        int timeout = 0;
        while (now < 8 * 3600 * 2 && timeout < 10) {
            delay(500);
            Serial.print(".");
            now = time(nullptr);
            timeout++;
        }
        Serial.println();
    }

    if (now >= 8 * 3600 * 2) {
        Serial.println("\nTime synchronized successfully!");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            Serial.print("Current local time: ");
            Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S (%Z %z)");
        }
    } else {
        Serial.println("\nFailed to obtain local time");

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 20);
        tft.println("ERROR");
        tft.setTextSize(1);
        tft.println("Time sync failed");
    }
}

} // namespace systemInit