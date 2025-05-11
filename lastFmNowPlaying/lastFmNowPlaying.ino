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

void setup() {
    // --- Initialize serial monitor ---
    Serial.begin(115200);
    unsigned long serialStart = millis();
    while (!Serial && millis() - serialStart < 2000) { // Wait max 2 seconds for serial
         delay(10);
    }

    // --- Initialize TFT Display using LovyanGFX ---
    Serial.println("Initializing TFT with LovyanGFX...");
    tft.init();
    Serial.println("tft.init() finished.");

    Serial.println("Setting rotation...");
    tft.setRotation(1); // 1 - Vertical rotation, USB-C on the right side
    Serial.println("Rotation set.");

    tft.clear(TFT_BLACK);

    // Configure text defaults
    // tft.setUTF8Print(true);
    tft.setFont(&myPolishFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1.25);
    tft.setCursor(0, 0);
    tft.println("TFT Initialized (LovyanGFX).");
    String cpuFreqStr = "CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz";
    tft.println(cpuFreqStr);
    String freeRamStr = "Free RAM: " + String(ESP.getFreeHeap()) + " bytes";
    tft.println(freeRamStr);
    Serial.println("TFT basics set up.");

    // --- Connect to Wi-Fi ---
    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(WIFI_SSID);
    tft.println("Connecting to WiFi...");
    tft.print(WIFI_SSID);

    // Begin WiFi connection
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

    // --- WiFi Connected Successfully ---
    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // --- Add NTP Time Synchronization ---
    Serial.println("Configuring time using NTP...");
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "europe.pool.ntp.org");
    Serial.print("Waiting for NTP time sync");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) { // Wait until time is reasonably beyond epoch (e.g., > 16 hours)
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

    // Display connection success on TFT
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("WiFi Connected!");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("IP: ");
    tft.println(WiFi.localIP());

    delay(1000);

    // --- Fetch Initial Data ---
    Serial.println("Fetching Now Playing data...");
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.println("Getting Last.fm data...");
    getNowPlaying();
    Serial.println("Initial data fetch attempt complete.");
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
        delay(REFRESH_MS); // Wait before potentially retrying in the next loop
        // TODO: Optionally restart the ESP
        // ESP.restart();
        return; // Skip fetching data if disconnected
    }
    Serial.printf("Free RAM: %u bytes\n", ESP.getFreeHeap());
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

            // Check if track changed before redrawing everything
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
    String albumCoverUrl = "";
    JsonArray images = track["image"];
    bool isPng = images[0]["#text"].as<String>().endsWith(".png");

    if (!images.isNull() && isPng) {
        // Prefer the largest
        Serial.println("Getting album cover url from last.fm API response...");
        if (images.size() > 3) albumCoverUrl = images[3]["#text"].as<String>(); // Try extralarge
        else if (images.size() > 2) albumCoverUrl = images[2]["#text"].as<String>(); // Try large
        else if (images.size() > 1) albumCoverUrl = images[1]["#text"].as<String>(); // Fallback to medium
        else albumCoverUrl = images[0]["#text"].as<String>();
    }
    else if (track.containsKey("album") && track["album"].is<JsonObject>() && track["album"].containsKey("mbid")) {
        Serial.println("Trying to getting album cover url from cover album art API response...");
        String mbid = track["album"]["mbid"].as<String>();

        if (mbid.length() > 0) {
            String albumApiPath = String("http://") + COVERALBUM_HOST + coverAlbumPath(mbid);

            WiFiClientSecure client;
            HTTPClient httpClient;

            Serial.println("Fetching from CAA: " + albumApiPath);

            DynamicJsonDocument doc = fetchJson(albumApiPath.c_str());

            if (doc.isNull()) {
                Serial.println("Failed to fetch JSON data from CAA.");
                return albumCoverUrl;
            }
            JsonObject root = doc.as<JsonObject>();
            if (root.containsKey("images") && root["images"].is<JsonArray>() && root["images"].size() > 0) {
                JsonObject firstImage = root["images"][0];
                String initialCoverUrl = "";
                if (firstImage.containsKey("thumbnails") && firstImage["thumbnails"].is<JsonObject>() && firstImage["thumbnails"].containsKey("small")) {
                    initialCoverUrl = firstImage["thumbnails"]["small"].as<String>();
                    albumCoverUrl = findFinalImageUrl(initialCoverUrl.c_str());
                    Serial.println("Found CAA thumbnail URL: " + albumCoverUrl);
                } else if (firstImage.containsKey("thumbnails") && firstImage["thumbnails"].is<JsonObject>() && firstImage["thumbnails"].containsKey("250")) { // Example fallback
                    initialCoverUrl = firstImage["thumbnails"]["250"].as<String>();
                    albumCoverUrl = findFinalImageUrl(initialCoverUrl.c_str());
                    Serial.println("Found CAA thumbnail URL (250px): " + albumCoverUrl);
                } else {
                    Serial.println("CAA JSON response missing 'images[0].thumbnails.small' or '.250'");
                }
            } else {
                Serial.println("CAA JSON response missing 'images' array or it's empty.");
            }
        } else {
            Serial.println("Album MBID is present but empty.");
        }
    } else {
        Serial.println("Track JSON missing 'album.mbid' needed for Cover Art Archive lookup.");
    }
    return albumCoverUrl;
}

void displayPlayIcon() {
  int x = SCREEN_WIDTH_PX - PLAYICON_PX - PLAYICON_PADDING_PX;
  int y = PLAYICON_PADDING_PX;
  tft.drawPngUrl(PLAYICON_URL, x, y);
}

void displayAlbumCover(String coverUrl) {
    bool success = false;
    String failText = "Load Fail";
    String functionAttempted = "N/A";

    if (coverUrl.endsWith(".jpg") || coverUrl.endsWith(".jpeg")) {
        failText = "JPG Failed";
        functionAttempted = "tft.drawJpgUrl";
        Serial.println("Attempting " + functionAttempted + "...");
        int EXPECTED_JPG_COVER_SIZE_PX = 250;
        float scale = ALBUM_COVER_SIZE_PX / EXPECTED_JPG_COVER_SIZE_PX;
        success = tft.drawJpgUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, ALBUM_PADDING_Y_PX, 0, 0, 0, 0, scale, scale);

    } else if (coverUrl.endsWith(".png")) {
        failText = "PNG Failed";
        functionAttempted = "tft.drawPngUrl";
        Serial.println("Attempting " + functionAttempted + "...");
        int EXPECTED_PNG_COVER_SIZE_PX = 300;
        float scale = ALBUM_COVER_SIZE_PX / EXPECTED_PNG_COVER_SIZE_PX;
        success = tft.drawPngUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, ALBUM_PADDING_Y_PX, 0, 0, 0, 0, scale, scale);

    } else {
        Serial.println("Unknown image type based on URL: " + coverUrl);
        failText = "Type Failed";
        functionAttempted = "No draw function applicable";
        success = false;
    }

    if (success) {
        Serial.println(functionAttempted + " reported SUCCESS.");
    } else if (functionAttempted == "tft.drawJpgUrl" || functionAttempted == "tft.drawPngUrl") {
        Serial.println(functionAttempted + " reported FAILURE.");
        Serial.println("Failure detail: " + failText);
    } else {
        Serial.println("Image drawing skipped/failed due to unknown type.");
        Serial.println("Failure detail: " + failText);
    }
}

void displayTrackInfo(String artistName, String songName, String albumName) {
    tft.setCursor(TEXT_LEFT_PADDING_PX, TEXT_START_HEIGHT_PX);
    tft.setFont(&fonts::Font0);
    tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextSize(TRACK_INFO_LABEL_TEXT_SIZE); tft.println("Artist:");
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    tft.setFont(&myPolishFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(TRACK_INFO_TEXT_SIZE); tft.println(artistName);
    tft.setTextSize(TRACK_INFO_SPACE_SIZE);
    tft.println();
    
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    tft.setFont(&fonts::Font0);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setTextSize(TRACK_INFO_LABEL_TEXT_SIZE); tft.println("Track:");
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    tft.setFont(&myPolishFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(TRACK_INFO_TEXT_SIZE); tft.println(songName);
    tft.setTextSize(TRACK_INFO_SPACE_SIZE);
    tft.println();

    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    tft.setFont(&fonts::Font0);
    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextSize(TRACK_INFO_LABEL_TEXT_SIZE); tft.println("Album:");
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    tft.setFont(&myPolishFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(TRACK_INFO_TEXT_SIZE); tft.println(albumName);
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