#include "WiFiManager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "LGFX.h"

extern LGFX tft;

namespace wifi {

static const unsigned long SERIAL_WAIT_MS = 2000;
static const int MAX_WIFI_ATTEMPTS = 3;
static const char* NTP_POOL[] = { "pool.ntp.org", "europe.pool.ntp.org" };
static const char* TZ_STRING = "CET-1CEST,M3.5.0,M10.5.0/3";

/** Outbound IPv4 seen by the internet */
static String fetchPublicIpv4() {
    HTTPClient http;
    if (!http.begin("http://api.ipify.org")) {
        return "";
    }
    http.setTimeout(10000);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return "";
    }
    String ip = http.getString();
    http.end();
    ip.trim();
    return ip;
}

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
        delay(5000);
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

    String localIp = WiFi.localIP().toString();
    Serial.print("Local IP: ");
    Serial.println(localIp);

    String gatewayIp = WiFi.gatewayIP().toString();
    Serial.print("Gateway IP: ");
    Serial.println(gatewayIp);

    String dnsIp = WiFi.dnsIP().toString();
    Serial.print("DNS IP: ");
    Serial.println(dnsIp);

    const String publicIp = fetchPublicIpv4();
    Serial.print("Public IP (WAN): ");
    Serial.println(publicIp.length() > 0 ? publicIp : "(fetch failed)");

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("WiFi Connected!");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.print(F("Local: "));
    tft.println(localIp);

    tft.print(F("Public: "));
    if (publicIp.length() > 0) {
        tft.println(publicIp);
    } else {
        tft.println(F("(unknown)"));
    }

    tft.print(F("Gateway: "));
    tft.println(gatewayIp);

    tft.print(F("DNS: "));
    tft.println(dnsIp);
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
