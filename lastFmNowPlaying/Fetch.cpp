#include "fetch.h"
#include "apiConfig.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

static const int MAX_REDIRECTS = 8;

DynamicJsonDocument fetchJson(const char* initialUrl) {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[fetch] WiFi not connected.");
        return doc;
    }

    HTTPClient http;
    String currentUrl = initialUrl;
    int redirectCount = 0;

    while (redirectCount <= MAX_REDIRECTS) {
        if (!http.begin(currentUrl)) {
            if (http.connected()) http.end();
            return doc;
        }
        http.setUserAgent("ESP32-HTTP-Client");
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            WiFiClient* stream = http.getStreamPtr();
            if (stream) {
                deserializeJson(doc, *stream);
            }
            http.end();
            return doc;
        }

        if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND ||
            httpCode == HTTP_CODE_TEMPORARY_REDIRECT || httpCode == 308) {
            String newUrl = http.getLocation();
            http.end();
            if (newUrl.length() == 0 || newUrl == currentUrl) return doc;
            currentUrl = newUrl;
            redirectCount++;
            continue;
        }

        if (httpCode > 0) {
            http.end();
        } else if (http.connected()) {
            http.end();
        }
        return doc;
    }
    if (http.connected()) http.end();
    return doc;
}

static String findFinalImageUrl(const char* initialUrl) {
    if (WiFi.status() != WL_CONNECTED) return "";
    String currentUrl = initialUrl;
    HTTPClient http;
    int redirectCount = 0;

    while (redirectCount <= MAX_REDIRECTS) {
        if (!http.begin(currentUrl)) {
            if (http.connected()) http.end();
            return "";
        }
        int httpCode = http.sendRequest("HEAD", (uint8_t*)nullptr, (size_t)0);
        if (httpCode == HTTP_CODE_OK) {
            http.end();
            return currentUrl;
        }
        if (httpCode == 301 || httpCode == 302 || httpCode == 307 || httpCode == 308) {
            String newUrl = http.getLocation();
            http.end();
            if (newUrl.length() == 0 || newUrl == currentUrl) return "";
            currentUrl = newUrl;
            redirectCount++;
        } else {
            http.end();
            return "";
        }
    }
    if (http.connected()) http.end();
    return "";
}

static String getConvertedImageUrl(const String& imageUrl, const String& mbid,
                                   const String& artist, const String& album) {
    if (WiFi.status() != WL_CONNECTED) return "";
    StaticJsonDocument<1024> jsonDoc;
    jsonDoc["imageUrl"] = imageUrl;
    if (mbid.length() > 0) jsonDoc["mbid"] = mbid;
    if (artist.length() > 0) jsonDoc["artist"] = artist;
    if (album.length() > 0) jsonDoc["album"] = album;

    String requestBody;
    serializeJson(jsonDoc, requestBody);

    HTTPClient http;
    if (!http.begin(JPG_CONVERTER_URL)) return "";
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", JPG_CONVERTER_URL_API_KEY);
    http.addHeader("Connection", "close");
    int httpCode = http.POST(requestBody);
    String result;
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        result = http.getString();
    }
    http.end();
    return result;
}

static String getMusicbrainzImageUrl(const String& mbid) {
    if (mbid.length() == 0) return "";
    String url = String("http://") + COVERALBUM_HOST + coverAlbumPath(mbid);
    DynamicJsonDocument doc = fetchJson(url.c_str());
    if (doc.isNull()) return "";

    JsonObject root = doc.as<JsonObject>();
    if (!root.containsKey("images") || !root["images"].is<JsonArray>() || root["images"].size() == 0) {
        return "";
    }
    JsonObject firstImage = root["images"][0];
    if (!firstImage.containsKey("thumbnails") || !firstImage["thumbnails"].is<JsonObject>()) {
        return "";
    }
    JsonObject thumbs = firstImage["thumbnails"];
    const char* key = thumbs.containsKey("small") ? "small" : "250";
    if (!thumbs.containsKey(key)) return "";
    String initialUrl = firstImage["thumbnails"][key].as<String>();
    return findFinalImageUrl(initialUrl.c_str());
}

static String getSuitableAlbumCoverUrlFromLastFmApi(JsonArray images) {
    if (images.isNull() || images.size() == 0) return "";
    if (images.size() > 3) return images[3]["#text"].as<String>();
    if (images.size() > 2) return images[2]["#text"].as<String>();
    if (images.size() > 1) return images[1]["#text"].as<String>();
    return images[0]["#text"].as<String>();
}

String resolveAlbumCoverUrl(JsonObject track) {
    JsonArray images = track["image"];
    String albumCoverUrl = getSuitableAlbumCoverUrlFromLastFmApi(images);
    bool isPng = (images.size() > 0 && images[0]["#text"].as<String>().endsWith(".png"));

    if (!images.isNull() && isPng) {
        return albumCoverUrl;
    }
    if (strlen(JPG_CONVERTER_URL) > 0 && !images.isNull() && !isPng) {
        String mbid, artist, album;
        if (track.containsKey("album") && track["album"].is<JsonObject>()) {
            if (track["album"].containsKey("mbid")) mbid = track["album"]["mbid"].as<String>();
            if (track["album"].containsKey("#text")) album = track["album"]["#text"].as<String>();
        }
        if (track.containsKey("artist") && track["artist"].is<JsonObject>() && track["artist"].containsKey("#text")) {
            artist = track["artist"]["#text"].as<String>();
        }
        String converted = getConvertedImageUrl(albumCoverUrl, mbid, artist, album);
        if (converted.length() > 0) return converted;
    }
    if (track.containsKey("album") && track["album"].is<JsonObject>() && track["album"].containsKey("mbid")) {
        String mbid = track["album"]["mbid"].as<String>();
        return getMusicbrainzImageUrl(mbid);
    }
    return albumCoverUrl;
}
