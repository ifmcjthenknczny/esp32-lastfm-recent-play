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
        return;
    }

    Serial.println("JSON Received and Parsed. Extracting data...");

    JsonObject recenttracks = doc["recenttracks"];
    if (!recenttracks.isNull()) {
        JsonArray trackArray = recenttracks["track"];
        if (!trackArray.isNull() && trackArray.size() > 0) {
            JsonObject recentTrack = trackArray[0];

            bool isPlaying = recentTrack.containsKey("@attr") &&
                    recentTrack["@attr"].containsKey("nowplaying") &&
                    recentTrack["@attr"]["nowplaying"].as<String>() == "true";

            if (!isPlaying && recentTrack.containsKey("date") && recentTrack["date"].is<JsonObject>() && recentTrack["date"].containsKey("uts")) {
                lastActivityTime = recentTrack["date"]["uts"].as<unsigned long>();
            } else {
                lastActivityTime = time(nullptr);
            }

            sleepOrWakeDisplay(isPlaying, lastActivityTime, displayActive);

            String songName = recentTrack["name"] | "Unknown Track";
            String artistName = recentTrack["artist"]["#text"] | "Unknown Artist";
            
            if (!shouldUpdate(displayActive, artistName, songName)) {
                displayOrErasePlayIcon(isPlaying);
                return;
            }

            String albumName = recentTrack["album"]["#text"] | "Unknown Album";
            String newAlbumCoverUrl = getAlbumCoverUrl(recentTrack);
            
            updateDisplay(artistName, songName, albumName, newAlbumCoverUrl, isPlaying);

            lastDisplayedArtist = artistName;
            lastDisplayedTrack = songName;
        } else {
            Serial.println("No tracks found in response.");
            // Handle display for "no tracks" state
            if (lastDisplayedArtist != "" || lastDisplayedTrack != "") {
                tft.fillScreen(TFT_BLUE); tft.setTextColor(TFT_WHITE); tft.setTextSize(1.25);
                tft.setCursor(0, 0); tft.println("No recent tracks found.");
                return;
            }
        }
    } else {
        Serial.println("Key 'recenttracks' not found in JSON.");
    }
}

void sleepOrWakeDisplay(bool isPlaying, unsigned long lastActivityTime, bool isDisplayActive) {
    if (isPlaying && !isDisplayActive) {
        setDisplayActive(true);
        return;
    }

    if (!isPlaying && isDisplayActive) {
        time_t now_ts = time(nullptr);
        unsigned long elapsedSeconds = now_ts - lastActivityTime;
        Serial.printf("Time since last activity: %lu seconds\n", elapsedSeconds);

        if (elapsedSeconds > DISPLAY_OFF_MS / 1000) {
            Serial.printf("Timeout reached (%lu ms > %d ms). ", elapsedSeconds * 1000, DISPLAY_OFF_MS);
            setDisplayActive(false);
        }
    }
}

bool shouldUpdate(bool isDisplayActive, String artistName, String songName) {
    if (!isDisplayActive) {
        Serial.println("Display is OFF, skipping drawing.");
        return false;
    }
    if (artistName == lastDisplayedArtist && songName == lastDisplayedTrack) {
        Serial.println("Track has not changed. Skipping redraw.");
        return false;
    }
    Serial.println("Display is ON, proceeding with drawing...");
    return true;
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

void displayOrErasePlayIcon(bool isPlaying) {
  int x = SCREEN_WIDTH_PX - PLAYICON_PX - PLAYICON_PADDING_PX;
  int y = PLAYICON_PADDING_PX;
  if (isPlaying) {
    Serial.println("Track is currently playing.");
    tft.drawPngUrl(PLAYICON_URL, x, y);
    return;
  }
  Serial.println("Track is not playing at the moment.");
  tft.fillRect(x, y, PLAYICON_PX, PLAYICON_PX, TFT_BLACK);
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
    displayTrackInfoBlock("Track", songName, TFT_GOLD);
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    displayTrackInfoBlock("Album", albumName, TFT_CYAN);
}

void updateDisplay(String artistName, String songName, String albumName, String albumCoverUrl, bool isPlaying) {
    tft.startWrite();
    tft.fillScreen(TFT_BLACK);
    Serial.println("--- Now Playing ---");
    Serial.print("Artist: "); Serial.println(artistName);
    Serial.print("Track: "); Serial.println(songName);
    Serial.print("Album: "); Serial.println(albumName);
    Serial.print("Cover URL: "); Serial.println(albumCoverUrl);
    // TODO: Only redraw album if actually changed, use sprites for track info
    displayAlbumCover(albumCoverUrl);
    displayTrackInfo(artistName, songName, albumName);
    displayOrErasePlayIcon(isPlaying);
    tft.endWrite();
}

// TODO: tft.setBrightness doesn't work, look for function that will do this stuff
void setDisplayActive(bool newDisplayActive) {
    if (newDisplayActive && !displayActive) {
        Serial.println("Turning display ON");
        displayActive = true;
        lastDisplayedArtist = "";
        lastDisplayedTrack = "";
    } else if (!newDisplayActive && displayActive) {
        Serial.println("Turning display OFF");
        tft.clear(TFT_BLACK);
        displayActive = false;
    }
}