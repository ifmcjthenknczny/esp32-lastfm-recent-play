#include "WiFiManager.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "LGFX.h"

extern LGFX tft;

namespace wifi {

static const int MAX_WIFI_ATTEMPTS = 3;

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

void connect() {
    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(WIFI_SSID);
    tft.println("Connecting to WiFi...");
    tft.print(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;

    while (WiFi.status() != WL_CONNECTED) {
        delay(10000);
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
            while (1) {
                delay(100);
            }
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
    Serial.println(publicIp.isEmpty() ? "(fetchFailed)" : publicIp);

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("WiFi Connected!");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.print("Local: ");
    tft.println(localIp);

    tft.print("Public: ");
    if (publicIp.isEmpty()) {
        tft.println("(unknown)");
    } else {
        tft.println(publicIp);
    }

    tft.print("Gateway: ");
    tft.println(gatewayIp);

    tft.print("DNS: ");
    tft.println(dnsIp);
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