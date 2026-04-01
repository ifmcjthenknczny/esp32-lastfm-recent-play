#include "fetch.h"
#include "apiConfig.h"
#include "ConfigDecl.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

static const int MAX_REDIRECTS = 8;

static bool isRedirect(int code) {
    return (code == 301 || code == 302 || code == 307 || code == 308);
}

void fetchJson(const char* initialUrl, DynamicJsonDocument& outDoc) {
    outDoc.clear();
    yield();
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[fetch] WiFi not connected.");
        return;
    }

    String url = initialUrl;
    HTTPClient http;

    for (int n = 0; n <= MAX_REDIRECTS; n++) {
        yield();
        if (!http.begin(url)) {
            if (http.connected()) http.end();
            return;
        }
        http.setUserAgent("ESP32-HTTP-Client");
        int code = http.GET();
        yield();

        if (code == HTTP_CODE_OK) {
            WiFiClient* stream = http.getStreamPtr();
            if (stream) deserializeJson(outDoc, *stream);
            http.end();
            return;
        }
        else if (isRedirect(code)) {
            String next = http.getLocation();
            http.end();
            if (next.length() == 0 || next == url) return;
            url = next;
            continue;
        }
        else {
            String payload = http.getString();
            DynamicJsonDocument errDoc(512);
            if (!deserializeJson(errDoc, payload) && errDoc.containsKey("message")) {
                String msg = errDoc["message"].as<String>();
                if (errDoc.containsKey("error")) {
                    Serial.println("fetchJson: error " + String(code) + " - " + msg + " (error: " + String(errDoc["error"].as<int>()) + ")");
                } else {
                    Serial.println("fetchJson: error " + String(code) + " - " + msg);
                }
            } else {
                Serial.println("fetchJson: error " + String(code) + " " + http.errorToString(code));
            }
            http.end();
        }
    }
}

static String findFinalImageUrl(const char* initialUrl) {
    if (WiFi.status() != WL_CONNECTED) return "";

    String url = initialUrl;
    HTTPClient http;

    for (int n = 0; n <= MAX_REDIRECTS; n++) {
        if (!http.begin(url)) {
            if (http.connected()) http.end();
            return "";
        }

        int code = http.sendRequest("HEAD", (uint8_t*)nullptr, (size_t)0);
        if (code == HTTP_CODE_OK) {
            http.end();
            return url;
        }
        if (!isRedirect(code)) {
            http.end();
            return "";
        }

        String next = http.getLocation();
        http.end();
        if (next.length() == 0 || next == url) return "";
        url = next;
    }
    return "";
}

static String getConvertedImageUrl(const String& imageUrl, const String& mbid,
                                   const String& artist, const String& album) {
    if (WiFi.status() != WL_CONNECTED) return "";

    StaticJsonDocument<1024> doc;
    doc["imageUrl"] = imageUrl;
    if (mbid.length())   doc["mbid"]   = mbid;
    if (artist.length()) doc["artist"] = artist;
    if (album.length())  doc["album"]  = album;

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    if (!http.begin(JPG_CONVERTER_URL)) return "";
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", JPG_CONVERTER_URL_API_KEY);
    http.addHeader("Connection", "close");

    int code = http.POST(body);
    String out = (code == HTTP_CODE_OK || code == HTTP_CODE_CREATED) ? http.getString() : "";
    http.end();
    return out;
}

static DynamicJsonDocument docCaa(JSON_BUFFER_SIZE);  // reused for CAA fetch to avoid per-call allocation

static String getMusicbrainzImageUrl(const String& mbid) {
    if (mbid.length() == 0) return "";
    String url = String("http://") + COVERALBUM_HOST + coverAlbumPath(mbid);
    fetchJson(url.c_str(), docCaa);
    if (docCaa.isNull()) return "";

    JsonObject root = docCaa.as<JsonObject>();
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
    size_t idx = (images.size() > 3) ? 3 : (images.size() - 1);
    return images[idx]["#text"].as<String>();
}

static String trackStr(JsonObject track, const char* key1, const char* key2) {
    if (!track.containsKey(key1) || !track[key1].is<JsonObject>() || !track[key1].as<JsonObject>().containsKey(key2))
        return "";
    return track[key1][key2].as<String>();
}

String getAlbumCoverUrl(JsonObject track) {
    JsonArray images = track["image"];
    String url = getSuitableAlbumCoverUrlFromLastFmApi(images);

    bool hasImages = !images.isNull() && images.size() > 0;
    bool isPng = hasImages && images[0]["#text"].as<String>().endsWith(".png");

    if (hasImages && isPng) {
        return url;
    }

    if (hasImages && !isPng && strlen(JPG_CONVERTER_URL) > 0) {
        String converted = getConvertedImageUrl(
            url,
            trackStr(track, "album", "mbid"),
            trackStr(track, "artist", "#text"),
            trackStr(track, "album", "#text")
        );
        if (converted.length() > 0) {
            return converted;
        }
    }

    String mbid = trackStr(track, "album", "mbid");
    if (mbid.length() > 0) {
        String musicbrainzUrl = getMusicbrainzImageUrl(mbid);
        return musicbrainzUrl;
    }

    return url;
}
