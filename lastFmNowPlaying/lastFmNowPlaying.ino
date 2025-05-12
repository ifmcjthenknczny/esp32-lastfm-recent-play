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

LGFX tft;

// --- Global Variables ---
// To avoid unnecessary redraws if track hasn't changed
String lastDisplayedAlbumCoverUrl = "";
String lastDisplayedArtist = ""; 
String lastDisplayedTrack = "";
bool displayActive = true;
unsigned long lastActivityTime = 0;

// --- Function Prototypes ---
void getNowPlaying();
void displayAlbumCover(String coverUrl);
void setDisplayActive(bool active);

void initSerialMonitor () {
    Serial.begin(115200);
    unsigned long serialStart = millis();
    while (!Serial && millis() - serialStart < 2000) {
        // Wait max 2 seconds for serial
         delay(10);
    }
}

void initDisplay() {
    Serial.println("Initializing TFT with LovyanGFX...");
    tft.init();
    Serial.println("tft.init() finished.");

    Serial.println("Setting rotation...");
    tft.setRotation(1); // 1 - Vertical rotation, USB-C on the right side
    Serial.println("Rotation set.");

    tft.clear(TFT_BLACK);

    tft.setFont(&myExtendedFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1.25);
    tft.setCursor(0, 0);
    tft.println("TFT Initialized (LovyanGFX).");
    String cpuFreqStr = "CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz";
    tft.println(cpuFreqStr);
    String freeRamStr = "Free RAM: " + String(ESP.getFreeHeap()) + " bytes";
    tft.println(freeRamStr);
    Serial.println("TFT basics set up.");
}

void connectToWifi() {
    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(WIFI_SSID);
    tft.println("Connecting to WiFi...");
    tft.print(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    const int maxWifiAttempts = 10;

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        tft.print("."); // Print dots on TFT as well
        attempts++;

        if (attempts >= maxWifiAttempts) {
            Serial.println("\nFailed to connect to WiFi!");
            tft.fillScreen(TFT_RED);
            tft.setTextColor(TFT_WHITE);
            tft.setTextSize(1.25);
            tft.setCursor(0, 0);
            tft.println("WiFi Failed!");
            Serial.println("Halting execution.");
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

void synchronizeTime() {
    Serial.println("Configuring time using NTP...");
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "europe.pool.ntp.org");
    Serial.print("Waiting for NTP time sync");
    time_t now = time(nullptr);
    // Wait until time is reasonably beyond epoch (e.g., > 16 hours)
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
    lastActivityTime = now;
}

void fetchInitialData() {
    Serial.println("Fetching Now Playing data...");
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.println("Getting Last.fm data...");
    getNowPlaying();
    Serial.println("Initial data fetch attempt complete.");
}

void setup() {
    initSerialMonitor();
    initDisplay();
    connectToWifi();
    synchronizeTime();
    delay(1000);
    fetchInitialData();
}

void loop() {
    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Attempting to reconnect...");
        tft.fillScreen(TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(1);
        tft.setCursor(0, 0);
        tft.println("WiFi Lost! Reconnecting...");
        // TODO: add WiFi reconnection logic
        return; // Skip fetching data if disconnected
    }
    Serial.printf("Free RAM: %u bytes\n", ESP.getFreeHeap()); // Track free RAM
    Serial.println("Refreshing Last.fm data...");
    getNowPlaying();
    Serial.println("Waiting before next refresh...");
    delay(REFRESH_MS);
}

void getNowPlaying() {
    Serial.println("\nFetching Now Playing data...");
    String lastFmUrl = String("http://") + LASTFM_HOST + LASTFM_PATH;

    DynamicJsonDocument doc = fetchJson(lastFmUrl.c_str());

    // Check if the fetch and parse was successful
    if (doc.isNull()) {
        Serial.println("Failed to fetch or parse JSON data.");
        lastDisplayedArtist = "";
        lastDisplayedTrack = "";
        return;
    }

    Serial.println("JSON Received and Parsed. Extracting data...");

    JsonObject recenttracks = doc["recenttracks"];
    if (!recenttracks.isNull()) {
        JsonArray trackArray = recenttracks["track"];
        if (!trackArray.isNull() && trackArray.size() > 0) {
            JsonObject track = trackArray[0]; // Most recent track

            bool isPlaying = false;
            if (track.containsKey("@attr")) {
                JsonObject attr = track["@attr"];
                if (attr.containsKey("nowplaying") && attr["nowplaying"].as<String>() == "true") {
                    isPlaying = true;
                }
            }

            unsigned long trackTimestampUTS = 0;
            if (track.containsKey("date") && track["date"].is<JsonObject>() && track["date"].containsKey("uts")) {
                trackTimestampUTS = track["date"]["uts"].as<unsigned long>();
            } else if (isPlaying) {
                trackTimestampUTS = time(nullptr);
            }

            if (isPlaying) {
                Serial.println("Track is currently playing.");
                lastActivityTime = time(nullptr);
                if (!displayActive) {
                    setDisplayActive(true);
                }
                displayPlayIcon();
            } else {
                Serial.println("Track is not currently playing.");
                if (trackTimestampUTS > lastActivityTime) {
                    lastActivityTime = trackTimestampUTS;
                }
                time_t now_ts = time(nullptr);
                unsigned long elapsedSeconds = now_ts - lastActivityTime;
                Serial.printf("Time since last activity: %lu seconds\n", elapsedSeconds);

                if (displayActive && (elapsedSeconds * 1000) > DISPLAY_OFF_MS) {
                    Serial.printf("Timeout reached (%lu ms > %d ms). ", elapsedSeconds * 1000, DISPLAY_OFF_MS);
                    setDisplayActive(false);
                }
            }

            if (!displayActive) {
                Serial.println("Display is OFF, skipping drawing.");
                return;
            }

            Serial.println("Display is ON, proceeding with drawing...");

            String songName = track["name"] | "Unknown Track";
            String albumName = track["album"]["#text"] | "Unknown Album";
            String artistName = track["artist"]["#text"] | "Unknown Artist";

            // IMPORTANT! DO NOT ERASE OR WILL GIVE HIGHER AWS COSTS ON JPG CONVERTER
            // TODO: lastDisplayedAlbumCoverUrl.length() > 0
            if (artistName == lastDisplayedArtist && songName == lastDisplayedTrack) {
                Serial.println("Track has not changed. Skipping redraw.");
                return;
            }

            lastDisplayedArtist = artistName;
            lastDisplayedTrack = songName;

            String newAlbumCoverUrl = getAlbumCoverUrl(track);

            Serial.println("--- Now Playing ---");
            Serial.print("Artist: "); Serial.println(artistName);
            Serial.print("Track: "); Serial.println(songName);
            Serial.print("Album: "); Serial.println(albumName);
            Serial.print("Cover URL: "); Serial.println(newAlbumCoverUrl);

            tft.startWrite();
            tft.fillScreen(TFT_BLACK);

            if (newAlbumCoverUrl != "") {
                // TODO: newAlbumCoverUrl != lastDisplayedAlbumCoverUrl ?
                displayAlbumCover(newAlbumCoverUrl);
                lastDisplayedAlbumCoverUrl = newAlbumCoverUrl;
            } else {
                Serial.println("No valid album cover URL found.");
            }
            displayTrackInfo(artistName, songName, albumName);
            tft.endWrite();
        } else {
            Serial.println("No tracks found in response.");
            // Handle display for "no tracks" state
             if (lastDisplayedArtist != "" || lastDisplayedTrack != "") { // Update only if state changes
                tft.fillScreen(TFT_BLUE); tft.setTextColor(TFT_WHITE); tft.setTextSize(1.25);
                tft.setCursor(0, 0); tft.println("No recent tracks found.");
                lastDisplayedArtist = ""; lastDisplayedTrack = ""; lastDisplayedAlbumCoverUrl = "";
            }
        }
    } else {
        Serial.println("Key 'recenttracks' not found in JSON.");
        // Handle display for invalid JSON structure
        tft.fillScreen(TFT_RED); tft.setTextColor(TFT_WHITE); tft.setTextSize(1);
        tft.setCursor(0, 0); tft.println("Error: Invalid JSON structure from API");
        lastDisplayedArtist = ""; lastDisplayedTrack = ""; lastDisplayedAlbumCoverUrl = "";
    }
}

String getAlbumCoverUrl(JsonObject track) {
    JsonArray images = track["image"];
    bool isPng = images[0]["#text"].as<String>().endsWith(".png");
    String albumCoverUrl = getSuitableAlbumCoverUrlFromLastFmApi(images);

    if (!images.isNull() && isPng) {
        Serial.println("Getting album cover url from last.fm API response...");
        return albumCoverUrl;
    } else if (JPG_CONVERTER_URL && !images.isNull() && !isPng) {
        String mbid = "";
        String artist = "";
        String album = "";
        if (track.containsKey("album") && track["album"].is<JsonObject>()) {
            if (track["album"].containsKey("mbid")) {
                mbid = track["album"]["mbid"].as<String>();
            }
            if (track["album"].containsKey("#text")) {
                album = track["album"]["#text"].as<String>();
            }
        }
        if (track.containsKey("artist") && track["artist"].is<JsonObject>() && track["artist"].containsKey("#text")) {
            artist = track["artist"]["#text"].as<String>();
        }
        albumCoverUrl = getConvertedImageUrl(albumCoverUrl, mbid, artist, album);
    } else if (track.containsKey("album") && track["album"].is<JsonObject>() && track["album"].containsKey("mbid")) {
        String mbid = track["album"]["mbid"].as<String>();
        albumCoverUrl = getMusicbrainzImageUrl(mbid);
    } else {
        Serial.println("Fetching cover album image have not worked.");
    }
    return albumCoverUrl;
}

void displayPlayIcon() {
  int x = SCREEN_WIDTH_PX - PLAYICON_PX - PLAYICON_PADDING_PX;
  int y = PLAYICON_PADDING_PX;
  tft.drawPngUrl(PLAYICON_URL, x, y);
}

void displayAlbumCover(String coverUrl) {
    float scale = calculateAlbumCoverScale(coverUrl);
    if (coverUrl.endsWith(".jpg") || coverUrl.endsWith(".jpeg") || coverUrl.startsWith(JPG_CONVERTER_BUCKET_HOST)) {
        Serial.println("Attempting drawJpgUrl...");
        tft.drawJpgUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, ALBUM_PADDING_Y_PX, 0, 0, 0, 0, scale, scale);
    } else if (coverUrl.endsWith(".png")) {
        Serial.println("Attempting drawPngUrl...");
        tft.drawPngUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, ALBUM_PADDING_Y_PX, 0, 0, 0, 0, scale, scale);
    } else {
        Serial.println("Unknown image type based on URL: " + coverUrl);
    }
}

void displayTrackInfoBlock(String label, String info, int labelColor) {
    String trimmedInfo = adjustTrackInfo(info);
    tft.setFont(&fonts::Font0);
    tft.setTextColor(labelColor, TFT_BLACK); tft.setTextSize(TRACK_INFO_LABEL_TEXT_SIZE); tft.println(label + ":");
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    tft.setFont(&myExtendedFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(TRACK_INFO_TEXT_SIZE); tft.println(trimmedInfo);
    tft.setTextSize(TRACK_INFO_SPACE_SIZE); tft.println();
}

void displayTrackInfo(String artistName, String songName, String albumName) {
    tft.setCursor(TEXT_LEFT_PADDING_PX, TEXT_START_HEIGHT_PX);
    displayTrackInfoBlock("Artist", artistName, TFT_RED);
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    displayTrackInfoBlock("Track", songName, TFT_YELLOW);
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    displayTrackInfoBlock("Album", albumName, TFT_CYAN);
}

void setDisplayActive(bool active) {
    if (active && !displayActive) {
        Serial.println("Turning display ON");
        tft.setBrightness(255);
        displayActive = true;
        lastDisplayedArtist = "";
        lastDisplayedTrack = "";
        lastDisplayedAlbumCoverUrl = "";
    } else if (!active && displayActive) {
        Serial.println("Turning display OFF");
        tft.clear(TFT_BLACK);
        tft.setBrightness(0);
        displayActive = false;
    }
}