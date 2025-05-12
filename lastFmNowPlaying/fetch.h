#ifndef FETCH_H
#define FETCH_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> 
#include <WiFiClientSecure.h>
#include "apiConfig.h"

const int MAX_REDIRECTS = 8;

DynamicJsonDocument fetchJson(const char* initialUrl) {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Błąd: Brak połączenia WiFi w fetchJson.");
        return doc;
    }

    HTTPClient http;
    String currentUrl = initialUrl;
    int redirectCount = 0;

    // Loop to handle redirects
    while (redirectCount <= MAX_REDIRECTS) {
        Serial.print("[fetchJson] Żądanie HTTP do: ");
        Serial.println(currentUrl);

        if (!http.begin(currentUrl)) {
            Serial.println("[fetchJson] Błąd: Nie można zainicjować HTTPClient (nieprawidłowy URL?)");
             if(http.connected()) http.end();
            return doc; 
        }

        http.setUserAgent("ESP32-HTTP-Client");

        int httpCode = http.GET();

        Serial.printf("[fetchJson] Kod odpowiedzi HTTP: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK) {
            // Success (200 OK) - parse the JSON
            Serial.println("[fetchJson] Otrzymano 200 OK. Przetwarzanie odpowiedzi...");
            WiFiClient* stream = http.getStreamPtr();

            if (stream) {
                DeserializationError error = deserializeJson(doc, *stream);
                if (error) {
                    Serial.print("[fetchJson] deserializeJson() nie powiodło się: ");
                    Serial.println(error.c_str());
                } else {
                    Serial.println("[fetchJson] Parsowanie JSON zakończone sukcesem.");
                }
            } else {
                Serial.println("[fetchJson] Błąd: Nie można uzyskać strumienia odpowiedzi.");
            }
            http.end();
            return doc;

        } else if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || // 301
                   httpCode == HTTP_CODE_FOUND ||            // 302
                   httpCode == HTTP_CODE_TEMPORARY_REDIRECT || // 307
                   httpCode == 308 /* Permanent Redirect */) {

            // Handle redirect
            Serial.printf("[fetchJson] Otrzymano kod przekierowania: %d\n", httpCode);

            String newUrl = http.getLocation();

            if (newUrl == nullptr || newUrl.length() == 0) { // Sprawdź, czy getLocation() coś zwróciło
                 Serial.println("[fetchJson] Błąd: http.getLocation() nie zwróciło poprawnego URL (brak nagłówka Location?).");
                 http.end();
                 return doc;
            }
             Serial.print("[fetchJson] Przekierowanie do: ");
             Serial.println(newUrl);

             http.end(); // Finish current connection before initiating new

            if (newUrl == currentUrl) {
                 Serial.println("[fetchJson] Błąd: Taki sam URL przekierowania - przerwanie pętli.");
                 return doc;
             }

             currentUrl = newUrl;
             redirectCount++;

             if (redirectCount > MAX_REDIRECTS) {
                 Serial.printf("[fetchJson] Błąd: Przekroczono maksymalną liczbę przekierowań (%d).\n", MAX_REDIRECTS);
                 return doc;
             }

        } else if (httpCode > 0) {
            Serial.printf("[fetchJson] Żądanie GET nie powiodło się po stronie serwera, kod błędu: %d\n", httpCode);
            String payload = http.getString();
            Serial.println("Treść odpowiedzi serwera (błąd):");
            Serial.println(payload);
            http.end();
            return doc;
        } else {
            // Client error - httpCode < 0
            Serial.printf("[fetchJson] Żądanie GET nie powiodło się, błąd HTTPClient: %s\n", http.errorToString(httpCode).c_str());
             if(http.connected()) http.end(); // In case connection is still not closed
            return doc;
        }
    }

    Serial.println("[fetchJson] Błąd: Pętla przekierowań zakończona nieoczekiwanie (powinna zwrócić wcześniej).");
    if(http.connected()) http.end(); // In case connection is still not closed
    return doc;
}

String findFinalImageUrl(const char* initialUrl) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[findFinalUrl] Błąd: Brak połączenia WiFi.");
        return "";
    }

    String currentUrl = initialUrl;
    HTTPClient http;
    int redirectCount = 0;

    while (redirectCount <= MAX_REDIRECTS) {
        Serial.printf("[findFinalUrl] Sprawdzam URL: %s\n", currentUrl.c_str());

         if (!http.begin(currentUrl)) {
            Serial.println("[findFinalUrl] Błąd: Nie można zainicjować HTTPClient.");
            if(http.connected()) http.end();
            return "";
        }

        // Using faster HEAD method, because we want headers and status code
        int httpCode = http.sendRequest("HEAD", (uint8_t*)nullptr, (size_t)0);

        Serial.printf("[findFinalUrl] Kod odpowiedzi HTTP: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK) {
            Serial.println("[findFinalUrl] Znaleziono ostateczny URL (200 OK).");
            http.end();
            return currentUrl;

        } else if (httpCode > 0 && (httpCode == 301 || httpCode == 302 || httpCode == 307 || httpCode == 308)) {
            // Handle redirect
            String newUrl = http.getLocation();
            http.end();

            if (newUrl == nullptr || newUrl.length() == 0) {
                 Serial.println("[findFinalUrl] Błąd: Brak nagłówka Location w przekierowaniu.");
                 return "";
            }
            if (newUrl == currentUrl) {
                 Serial.println("[findFinalUrl] Błąd: Taki sam URL przekierowania.");
                 return "";
            }

            Serial.printf("[findFinalUrl] Przekierowanie do: %s\n", newUrl.c_str());
            currentUrl = newUrl; // New redirect url
            redirectCount++;

            if (redirectCount > MAX_REDIRECTS) {
                Serial.printf("[findFinalUrl] Błąd: Przekroczono limit przekierowań (%d).\n", MAX_REDIRECTS);
                return "";
            }
        } else {
            Serial.printf("[findFinalUrl] Błąd HTTP %d lub błąd klienta: %s\n", httpCode, http.errorToString(httpCode).c_str());
            http.end();
            return "";
        }
    }

    Serial.println("[findFinalUrl] Nie udało się znaleźć ostatecznego URL w ramach limitu przekierowań.");
    if(http.connected()) http.end(); // In case it is not closed yet
    return "";
}

String getConvertedImageUrl(String imageUrl, String mbid = "", String artist = "", String album = "") {
  Serial.println("Trying to get album cover url from API coverting progressive JPG to baseline JPG...");
  String responseUrl = "";
  const int jsonCapacity = 1024;
  StaticJsonDocument<jsonCapacity> jsonDoc;
  jsonDoc["imageUrl"] = imageUrl;

  if (mbid != NULL && mbid.length() > 0) {
    jsonDoc["mbid"] = mbid;
  }
  if (artist != NULL && artist.length() > 0) {
    jsonDoc["artist"] = artist;
  }
  if (album != NULL && album.length() > 0) {
    jsonDoc["album"] = album;
  }

  String requestBody;
  serializeJson(jsonDoc, requestBody);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    if (http.begin(JPG_CONVERTER_URL)) {
      http.addHeader("Content-Type", "application/json");
      http.addHeader("x-api-key", JPG_CONVERTER_URL_API_KEY);
      http.addHeader("Connection", "close");

      int httpCode = http.POST(requestBody);
      
      if (httpCode > 0) {
        String responsePayload = http.getString();
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
          responseUrl = responsePayload;
        } else {
          Serial.println("Error: Server responded with HTTP code " + String(httpCode));
          Serial.println("Response: " + responsePayload);
        }
      } else {
        Serial.println("Error on POST request. HTTP Code: " + String(httpCode) + ", Error: " + http.errorToString(httpCode).c_str());
      }
      http.end();
    } else {
      Serial.println("Error: http.begin() failed. Could not connect to server.");
    }
  } else {
    Serial.println("Error: WiFi not connected.");
  }
  return responseUrl;
}

String getMusicbrainzImageUrl(String mbid = "") {
    String albumCoverUrl = "";
    Serial.println("Trying to get album cover url from cover album art API...");
    String albumApiPath = String("http://") + COVERALBUM_HOST + coverAlbumPath(mbid);

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
}

String getSuitableAlbumCoverUrlFromLastFmApi(JsonArray images) {
    String albumCoverUrl = "";
    if (images.isNull()) {
        return albumCoverUrl;
    }
    // Prefer the largest
    if (images.size() > 3) albumCoverUrl = images[3]["#text"].as<String>(); // Try extralarge
    else if (images.size() > 2) albumCoverUrl = images[2]["#text"].as<String>(); // Try large
    else if (images.size() > 1) albumCoverUrl = images[1]["#text"].as<String>(); // Fallback to medium
    else albumCoverUrl = images[0]["#text"].as<String>();
    return albumCoverUrl;
}

#endif