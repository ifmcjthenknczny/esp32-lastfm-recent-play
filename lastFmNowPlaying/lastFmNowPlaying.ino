#include <time.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "LGFX.h"
#include "config.h"
#include "apiConfig.h"
#include "helpers.h"

LGFX tft;

// --- Global Variables ---
String albumCoverUrl = ""; // To store the URL of the album cover
String lastDisplayedArtist = ""; // To avoid unnecessary redraws if track hasn't changed
String lastDisplayedTrack = "";

// --- Function Prototypes ---
void getNowPlaying();
void displayAlbumCover(String coverUrl);

//====================================================================================
// SETUP
//====================================================================================
void setup() {
    // --- Initialize Serial Monitor ---
    Serial.begin(115200);
    unsigned long serialStart = millis();
    while (!Serial && millis() - serialStart < 2000) { // Wait max 2 seconds for serial
         delay(10);
    }
    Serial.println("\n--- Sketch Start ---"); // Initial message to confirm Serial works

    // --- Initialize TFT Display using LovyanGFX ---
    Serial.println("Initializing TFT with LovyanGFX...");
    tft.init();
    Serial.println("tft.init() finished.");

    Serial.println("Setting rotation...");
    tft.setRotation(1); // 1 - Vertical rotation, USB-C on the right side
    Serial.println("Rotation set.");

    Serial.println("Filling screen black...");
    tft.fillScreen(TFT_BLACK); // Clear screen to black immediately after init
    Serial.println("fillScreen finished.");

    // Configure text defaults
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set default text color
    tft.setTextSize(1);                      // Set default text size (adjust scaling if needed)
    tft.setCursor(10, 10);                   // Set initial cursor position
    tft.println("TFT Initialized (LovyanGFX)."); // Display initial status on TFT
    Serial.println("TFT basics set up.");

    // --- Connect to Wi-Fi ---
    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(WIFI_SSID);
    tft.println("Connecting to WiFi...");
    tft.print(WIFI_SSID);                  // Show SSID on TFT

    // Begin WiFi connection
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    const int maxWifiAttempts = 10; // Define max attempts as a constant
    tft.setCursor(10, tft.getCursorY() + 5); // Move cursor for dots

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        tft.print("."); // Print dots on TFT as well
        attempts++;

        if (attempts >= maxWifiAttempts) {
            Serial.println("\nFailed to connect to WiFi!");
            tft.fillScreen(TFT_RED); // Use color to indicate error
            tft.setTextColor(TFT_WHITE);
            tft.setTextSize(2);
            tft.setCursor(10, 10);
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
    if (getLocalTime(&timeinfo)) { // Use getLocalTime after configTzTime
        Serial.print("Current local time: ");
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S (%Z %z)"); // Print formatted time
    } else {
        Serial.println("Failed to obtain local time");
    }

    // Display connection success on TFT
    tft.fillScreen(TFT_BLACK); // Clear screen before showing final status
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_GREEN, TFT_BLACK); // Green text for success
    tft.println("WiFi Connected!");
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Back to default color
    tft.print("IP: ");
    tft.println(WiFi.localIP());

    Serial.println("Waiting briefly...");
    delay(2000); // Show connection info briefly on TFT

    // --- Fetch Initial Data ---
    Serial.println("Fetching Now Playing data...");
    tft.fillScreen(TFT_BLACK); // Clear screen
    tft.setCursor(10, 10);
    tft.println("Getting Last.fm data...");
    getNowPlaying(); // Call the function to get data from Last.fm
    Serial.println("Initial data fetch attempt complete.");
}

//====================================================================================
// LOOP
//====================================================================================
void loop() {
    // Check WiFi status periodically
    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Attempting to reconnect...");
        tft.fillScreen(TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(1);
        tft.setCursor(10, 10);
        tft.println("WiFi Lost! Reconnecting...");
        // TODO: add WiFi reconnection logic
        delay(5000); // Wait before potentially retrying in the next loop
        // TODO: Optionally restart the ESP
        // ESP.restart();
        return; // Skip fetching data if disconnected
    }

    Serial.println("Refreshing Last.fm data...");
    getNowPlaying();
    Serial.println("Waiting before next refresh...");
    delay(3000); // Refresh every 3 seconds
}

String getAlbumCoverUrl(JsonObject track) {
    albumCoverUrl = "";
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
            String albumApiPath = coverAlbumPath(mbid);

            WiFiClientSecure client;
            HTTPClient httpClient;

            // Optional: Allow insecure connections if certificate validation fails (use with caution)
            // client.setInsecure();

            Serial.println("Fetching from CAA: " + albumApiPath);

            String jsonBody = fetchJsonOverHttps(COVERALBUM_HOST, COVERALBUM_PORT, albumApiPath.c_str());

            if (jsonBody.length() > 0) {
                // We got a response body, proceed with JSON parsing
                Serial.println("Manual HTTPS fetch succeeded. Parsing JSON...");
                DynamicJsonDocument doc(JSON_BUFFER_SIZE);

                DeserializationError error = deserializeJson(doc, jsonBody); // Parse the String body

                if (!error) {
                    JsonObject root = doc.as<JsonObject>();
                    if (root.containsKey("images") && root["images"].is<JsonArray>() && root["images"].size() > 0) {
                        JsonObject firstImage = root["images"][0];
                        if (firstImage.containsKey("thumbnails") && firstImage["thumbnails"].is<JsonObject>() && firstImage["thumbnails"].containsKey("small")) {
                            albumCoverUrl = firstImage["thumbnails"]["small"].as<String>();
                            Serial.println("Found CAA thumbnail URL: " + albumCoverUrl);
                        } else if (firstImage.containsKey("thumbnails") && firstImage["thumbnails"].is<JsonObject>() && firstImage["thumbnails"].containsKey("250")) { // Example fallback
                            albumCoverUrl = firstImage["thumbnails"]["250"].as<String>();
                            Serial.println("Found CAA thumbnail URL (250px): " + albumCoverUrl);
                        } else {
                            Serial.println("CAA JSON response missing 'images[0].thumbnails.small' or '.250'");
                        }
                    } else {
                        Serial.println("CAA JSON response missing 'images' array or it's empty.");
                    }
                } else {
                    // JSON Parsing Failed
                    Serial.print("deserializeJson() failed: ");
                    Serial.println(error.c_str());
                    Serial.println("Received body was:");
                    Serial.println(jsonBody);
                }
            } else {
                Serial.println("Failed to fetch JSON data from CAA using manual HTTPS (check logs from fetchJsonOverHttps).");
            }
        } else {
            Serial.println("Album MBID is present but empty.");
        }
    } else {
        Serial.println("Track JSON missing 'album.mbid' needed for Cover Art Archive lookup.");
    }
    return albumCoverUrl;
}

//====================================================================================
// Get Now Playing Data
//====================================================================================
void getNowPlaying() {
    WiFiClient client; // Create a WiFiClient object for the API request

    Serial.print("Connecting to Last.fm host: ");
    Serial.println(LASTFM_HOST);

    // Connect to the server
    if (!client.connect(LASTFM_HOST, LASTFM_PORT)) {
        Serial.println("Connection to Last.fm failed!");
        // Avoid filling the whole screen if connection just blips
        // Consider a small indicator instead? Or just log it.
        // tft.fillScreen(TFT_ORANGE); ...
        Serial.println("Error connecting to Last.fm API");
        return;
    }
    Serial.println("Connected to host.");

    // Construct and send the HTTP GET request
    Serial.print("Requesting URL: ");
    Serial.println(LASTFM_PATH);

    client.print(String("GET ") + LASTFM_PATH + " HTTP/1.1\r\n" +
                 "Host: " + LASTFM_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");

    // --- Wait for response (with timeout) ---
    unsigned long timeout = millis();
    while (!client.available() && millis() - timeout < 5000) {
        delay(10); // Wait for data
    }

    if (!client.available()) {
         Serial.println("No response from server!");
         client.stop();
         // Avoid filling screen on transient error
         Serial.println("No response from Last.fm API");
         return;
    }

    // --- Read HTTP response ---
    String statusLine = "";
    bool headersDone = false;
    bool httpOk = false;
    unsigned long headerTimeout = millis(); // Separate timeout for headers

    // Read headers line by line
    while (client.connected() || client.available()) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            line.trim(); // Remove \r

            if (statusLine == "") {
                statusLine = line;
                Serial.print("Status: ");
                Serial.println(statusLine);
                if (statusLine.startsWith("HTTP/1.1 200 OK")) {
                    httpOk = true;
                } else {
                    Serial.println("HTTP Error!");
                    break; // Exit header reading on error
                }
            }

            // An empty line indicates the end of headers
            if (line.length() == 0) {
                headersDone = true;
                break; // Stop reading headers
            }
            headerTimeout = millis(); // Reset header timeout while receiving headers
        } else {
          delay(10); // Wait if buffer empty but still connected
        }
        // Timeout for reading headers specifically
        if (millis() - headerTimeout > 10000) {
           Serial.println("Timeout reading headers!");
           httpOk = false;
           break;
        }
    }

    // --- Process the response body (JSON) ---
    if (httpOk && headersDone) {
        Serial.println("Headers received, processing JSON body...");

        // Allocate the JSON document
        DynamicJsonDocument doc(JSON_BUFFER_SIZE);

        // Parse JSON directly from the client stream (memory efficient)
        DeserializationError error = deserializeJson(doc, client);

        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            tft.fillScreen(TFT_RED);
            tft.setTextColor(TFT_WHITE);
            tft.setTextSize(1);
            tft.setCursor(10, 10);
            tft.println("JSON Parsing Error:");
            tft.print(error.c_str());

        } else {
            Serial.println("JSON Parsed successfully.");

            // Extract data with checking whether key exist
            JsonObject recenttracks = doc["recenttracks"];
            if (!recenttracks.isNull()) {
                 JsonArray trackArray = recenttracks["track"];
                 if (!trackArray.isNull() && trackArray.size() > 0) {
                      JsonObject track = trackArray[0]; // Most recent track

                      // Use .as<String>() for safer extraction
                      String songName = track["name"].as<String>();
                      String albumName = track["album"]["#text"].as<String>();
                      String artistName = track["artist"]["#text"].as<String>();

                      // Check if track changed before redrawing everything
                      if (artistName == lastDisplayedArtist && songName == lastDisplayedTrack) {
                          Serial.println("Track has not changed. Skipping redraw.");
                          client.stop(); // Still need to stop the client
                          return;
                      }
                      // Update last displayed info
                      lastDisplayedArtist = artistName;
                      lastDisplayedTrack = songName;

                      albumCoverUrl = getAlbumCoverUrl(track);

                      Serial.println("--- Now Playing ---");
                      Serial.print("Artist: "); Serial.println(artistName);
                      Serial.print("Track: "); Serial.println(songName);
                      Serial.print("Album: "); Serial.println(albumName);
                      Serial.print("Cover URL: "); Serial.println(albumCoverUrl);

                      // --- Display on TFT ---
                      tft.startWrite(); // Batch drawing operations for performance
                      tft.fillScreen(TFT_BLACK); // Clear screen before drawing new info

                      int coverX = 0;
                      int coverY = 0;
                      int coverSize = 128; // Example size
                      int textStartY = 10; // Default start Y for text

                      if (albumCoverUrl != "" && albumCoverUrl.startsWith("http")) {
                           displayAlbumCover(albumCoverUrl);
                           textStartY = coverSize;
                      } else {
                           Serial.println("No valid album cover URL found.");
                      }

                      // --- Display Text Info ---
                      tft.setCursor(10, textStartY);

                      tft.setTextColor(TFT_CYAN, TFT_BLACK); // Artist color
                      tft.setTextSize(1); // Use LovyanGFX scaling if needed: tft.setTextScale(1);
                      tft.println("Artist:");
                      tft.setTextSize(2); // Or tft.setTextScale(2);
                      tft.setTextColor(TFT_WHITE, TFT_BLACK); // White color for value
                      tft.println(artistName);

                      tft.println(); // Add some space

                      tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Track color
                      tft.setTextSize(1);
                      tft.println("Track:");
                      tft.setTextSize(2);
                      tft.setTextColor(TFT_WHITE, TFT_BLACK);
                      tft.println(songName);

                      tft.println();

                      tft.setTextColor(TFT_GREEN, TFT_BLACK); // Album color
                      tft.setTextSize(1);
                      tft.println("Album:");
                      tft.setTextSize(1);
                      tft.setTextColor(TFT_WHITE, TFT_BLACK);
                      tft.println(albumName);

                      tft.endWrite(); // End batch drawing

                 } else {
                      Serial.println("No tracks found in response.");
                      // Only update screen if previous state wasn't also "no tracks"
                      if (lastDisplayedArtist != "" || lastDisplayedTrack != "") {
                          tft.fillScreen(TFT_BLUE);
                          tft.setTextColor(TFT_WHITE);
                          tft.setTextSize(2);
                          tft.setCursor(10, 10);
                          tft.println("No recent tracks found.");
                          lastDisplayedArtist = ""; // Reset state on error
                          lastDisplayedTrack = "";
                      }
                 }
            } else {
                 Serial.println("Key 'recenttracks' not found.");
                 tft.fillScreen(TFT_RED);
                 tft.setTextColor(TFT_WHITE);
                 tft.setTextSize(1);
                 tft.setCursor(10, 10);
                 tft.println("Error: Invalid JSON structure from Last.fm");
                 lastDisplayedArtist = ""; // Reset state on error
                 lastDisplayedTrack = "";
            }
        }
    } else {
        Serial.println("Failed to get valid HTTP response or headers.");
         if (!httpOk) {
             // Display brief error on screen only if HTTP status was bad
             tft.fillRect(0, tft.height() - 20, tft.width(), 20, TFT_RED); // Error bar at bottom
             tft.setTextColor(TFT_WHITE, TFT_RED);
             tft.setTextSize(1);
             tft.setCursor(5, tft.height() - 15);
             tft.print("HTTP Error: ");
             tft.print(statusLine.substring(0, 20));
             delay(2000);
         }
         // Reset state on error
         lastDisplayedArtist = ""; 
         lastDisplayedTrack = "";
    }

    // Disconnect the client
    Serial.println("Stopping API client.");
    client.stop();
}


//====================================================================================
// Display Album Cover
//====================================================================================
void displayAlbumCover(String coverUrl) {
    Serial.printf("Free RAM before draw: %u bytes\n", ESP.getFreeHeap());

    int x = 0; // Top-left X
    int y = 0; // Top-left Y
    bool success = false;
    String failText = "Load Fail";
    String functionAttempted = "N/A";

if (coverUrl.endsWith(".jpg") || coverUrl.endsWith(".jpeg")) {
    failText = "JPG Failed";
    functionAttempted = "tft.drawJpgUrl";
    Serial.println("Attempting " + functionAttempted + "...");
    success = tft.drawJpgUrl(coverUrl.c_str(), x, y);

} else if (coverUrl.endsWith(".png")) {
    failText = "PNG Failed";
    functionAttempted = "tft.drawPngUrl";
    Serial.println("Attempting " + functionAttempted + "...");
    success = tft.drawPngUrl(coverUrl.c_str(), x, y);

} else {
    Serial.println("Unknown image type based on URL: " + coverUrl);
    failText = "Type Failed";
    functionAttempted = "No draw function applicable";
    success = false;
}

if (success) {
    Serial.println(functionAttempted + " reported SUCCESS.");
} else {
    if (functionAttempted == "tft.drawJpgUrl" || functionAttempted == "tft.drawPngUrl") {
        Serial.println(functionAttempted + " reported FAILURE.");
    } else {
        Serial.println("Image drawing skipped/failed due to unknown type.");
    }
    Serial.println("Failure detail: " + failText);
}
     Serial.printf("Free RAM after draw: %u bytes\n", ESP.getFreeHeap());
}

void drawAlbumFailureFallback(int x, int y, String failText) {
    // Draw failure placeholder
    int coverSize = 128; // Match the size used elsewhere
    tft.fillRect(x, y, coverSize, coverSize, TFT_DARKGRAY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGRAY);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM); // Middle Center datum
    tft.drawString(failText, x + coverSize / 2, y + coverSize / 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Reset text color
    tft.setTextDatum(TL_DATUM); // Reset datum to Top Left (default)
}