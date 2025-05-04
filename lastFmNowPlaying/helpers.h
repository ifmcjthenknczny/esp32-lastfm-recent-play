#ifndef HELPERS_H
#define HELPERS_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> 
#include <WiFiClientSecure.h>
#include "apiConfig.h"

const int MAX_REDIRECTS = 5;

DynamicJsonDocument fetchJson(const char* initialUrl) {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE); // Tworzymy dokument na początku

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Błąd: Brak połączenia WiFi w fetchJson.");
        return doc; // Zwracamy pusty dokument. doc.isNull() będzie true.
    }

    HTTPClient http;
    String currentUrl = initialUrl; // Używamy String, aby móc modyfikować URL w pętli przekierowań
    int redirectCount = 0;

    // Pętla do obsługi przekierowań
    while (redirectCount <= MAX_REDIRECTS) {
        Serial.print("[fetchJson] Żądanie HTTP do: ");
        Serial.println(currentUrl);

        // Uwaga: Jeśli URL CAA jest HTTPS, HTTPClient powinien sobie poradzić.
        // W razie problemów z certyfikatem, można użyć http.begin(client, url) z skonfigurowanym WiFiClientSecure.
        // np. WiFiClientSecure client; client.setInsecure(); http.begin(client, currentUrl);
        if (!http.begin(currentUrl)) { // Inicjalizujemy żądanie dla aktualnego URL
            Serial.println("[fetchJson] Błąd: Nie można zainicjować HTTPClient (nieprawidłowy URL?)");
             if(http.connected()) http.end(); // Upewnij się, że jest zakończone jeśli begin zwróciło false po częściowej inicjalizacji
            return doc; // Zwracamy pusty dokument.
        }

        http.setUserAgent("ESP32-HTTP-Client");
        // Można dodać inne nagłówki, np. http.addHeader("Accept", "application/json");

        int httpCode = http.GET(); // Wykonujemy żądanie GET

        Serial.printf("[fetchJson] Kod odpowiedzi HTTP: %d\n", httpCode);

        // Sprawdzanie kodu odpowiedzi
        if (httpCode == HTTP_CODE_OK) {
            // Sukces (200 OK) - parsujemy JSON
            Serial.println("[fetchJson] Otrzymano 200 OK. Przetwarzanie odpowiedzi...");
            WiFiClient* stream = http.getStreamPtr();

            if (stream) {
                DeserializationError error = deserializeJson(doc, *stream);
                if (error) {
                    Serial.print("[fetchJson] deserializeJson() nie powiodło się: ");
                    Serial.println(error.c_str());
                    // W przypadku błędu, doc pozostanie nieprawidłowy (doc.isNull() zwróci true).
                } else {
                    Serial.println("[fetchJson] Parsowanie JSON zakończone sukcesem.");
                    // Sukces! doc zawiera dane
                }
            } else {
                Serial.println("[fetchJson] Błąd: Nie można uzyskać strumienia odpowiedzi.");
                // doc pozostanie nieprawidłowy.
            }
            http.end(); // Zakończ połączenie po sukcesie
            return doc; // Zwróć dokument (sparsowany lub pusty w razie błędu parsowania)

        } else if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || // 301
                   httpCode == HTTP_CODE_FOUND ||            // 302
                   httpCode == HTTP_CODE_TEMPORARY_REDIRECT || // 307
                   httpCode == 308 /* Permanent Redirect */) { // 308 (nie ma stałej w HTTPClient?)

            // Obsługa przekierowania
             Serial.printf("[fetchJson] Otrzymano kod przekierowania: %d\n", httpCode);

            String newUrl = http.getLocation(); // Spróbuj pobrać URL przekierowania bezpośrednio

            if (newUrl == nullptr || newUrl.length() == 0) { // Sprawdź, czy getLocation() coś zwróciło
                 Serial.println("[fetchJson] Błąd: http.getLocation() nie zwróciło poprawnego URL (brak nagłówka Location?).");
                 http.end();
                 return doc; // Błąd - brak URL do przekierowania
            }
             Serial.print("[fetchJson] Przekierowanie do: ");
             Serial.println(newUrl);

             http.end(); // WAŻNE: Zakończ bieżące połączenie przed rozpoczęciem nowego

            if (newUrl == currentUrl) {
                 Serial.println("[fetchJson] Błąd: Taki sam URL przekierowania - przerwanie pętli.");
                 return doc; // Zapobiega pętli jeśli serwer źle skonfigurowany
             }

             currentUrl = newUrl; // Ustaw nowy URL do następnej iteracji
             redirectCount++;     // Zwiększ licznik przekierowań

             if (redirectCount > MAX_REDIRECTS) {
                 Serial.printf("[fetchJson] Błąd: Przekroczono maksymalną liczbę przekierowań (%d).\n", MAX_REDIRECTS);
                 // Nie trzeba http.end(), bo zostało wywołane przed sprawdzeniem licznika
                 return doc; // Zwróć pusty dokument
             }

        } else if (httpCode > 0) {
            Serial.printf("[fetchJson] Żądanie GET nie powiodło się po stronie serwera, kod błędu: %d\n", httpCode);
            String payload = http.getString();
            Serial.println("Treść odpowiedzi serwera (błąd):");
            Serial.println(payload);
            http.end();
            return doc;

        } else {
            // Błąd po stronie klienta HTTP (np. problem z połączeniem, DNS) - httpCode < 0
            Serial.printf("[fetchJson] Żądanie GET nie powiodło się, błąd HTTPClient: %s\n", http.errorToString(httpCode).c_str());
            // Nie trzeba http.end() w tym przypadku, bo GET już zwrócił błąd klienta
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
        return ""; // Zwróć pusty string w razie braku WiFi
    }

    String currentUrl = initialUrl;
    HTTPClient http;
    int redirectCount = 0;

    while (redirectCount <= MAX_REDIRECTS) {
        Serial.printf("[findFinalUrl] Sprawdzam URL: %s\n", currentUrl.c_str());

        // Można rozważyć WiFiClientSecure jeśli wiemy, że będą HTTPS
        // WiFiClientSecure client; client.setInsecure(); // Używać ostrożnie!
        // if (!http.begin(client, currentUrl)) { ... }
         if (!http.begin(currentUrl)) {
            Serial.println("[findFinalUrl] Błąd: Nie można zainicjować HTTPClient.");
            if(http.connected()) http.end();
            return ""; // Zwróć pusty string w razie błędu
        }

        // Używamy metody HEAD - chcemy tylko nagłówki i kod statusu, nie cały obrazek!
        // To jest znacznie szybsze i oszczędza pamięć/dane.
        // Można też użyć GET, ale przerwać po odczytaniu nagłówków. HEAD jest czystsze.
        int httpCode = http.sendRequest("HEAD", (uint8_t*)nullptr, (size_t)0);
        // Alternatywnie, jeśli HEAD nie działa z serwerem:
        // int httpCode = http.GET();

        Serial.printf("[findFinalUrl] Kod odpowiedzi HTTP: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK) { // Znaleźliśmy 200 OK!
            Serial.println("[findFinalUrl] Znaleziono ostateczny URL (200 OK).");
            http.end();
            return currentUrl; // Ten URL jest ostateczny

        } else if (httpCode > 0 && (httpCode == 301 || httpCode == 302 || httpCode == 307 || httpCode == 308)) {
            // Obsługa przekierowania
            String newUrl = http.getLocation(); // Używamy getLocation()
            http.end(); // Zakończ bieżące połączenie

            if (newUrl == nullptr || newUrl.length() == 0) {
                 Serial.println("[findFinalUrl] Błąd: Brak nagłówka Location w przekierowaniu.");
                 return "";
            }
            if (newUrl == currentUrl) {
                 Serial.println("[findFinalUrl] Błąd: Taki sam URL przekierowania.");
                 return "";
            }

            Serial.printf("[findFinalUrl] Przekierowanie do: %s\n", newUrl.c_str());
            currentUrl = newUrl; // Aktualizuj URL
            redirectCount++;

            if (redirectCount > MAX_REDIRECTS) {
                Serial.printf("[findFinalUrl] Błąd: Przekroczono limit przekierowań (%d).\n", MAX_REDIRECTS);
                return "";
            }
            // Pętla będzie kontynuowana

        } else { // Inny błąd (np. 404, 500, błąd klienta < 0)
            Serial.printf("[findFinalUrl] Błąd HTTP %d lub błąd klienta: %s\n", httpCode, http.errorToString(httpCode).c_str());
            http.end();
            return ""; // Zwróć pusty string w razie błędu
        }

        // Jeśli używasz GET zamiast HEAD, musisz zakończyć połączenie tutaj,
        // chyba że chcesz odczytać ciało (czego nie chcemy)
        // if (httpCode != HTTP_CODE_OK && httpCode <= 0) { // Jeśli GET zwrócił błąd klienta
        //    http.end(); // Upewnij się, że jest zamknięte
        // } else if (httpCode > 0 && httpCode != HTTP_CODE_OK) {
             // Jeśli GET zwrócił kod inny niż 200 OK, ale poprawny (>0), np. przekierowanie
             // Nie kończymy tutaj, bo getLocation() potrzebuje aktywnego połączenia
        // }

    } // koniec while

    Serial.println("[findFinalUrl] Nie udało się znaleźć ostatecznego URL w ramach limitu przekierowań.");
    if(http.connected()) http.end(); // Na wszelki wypadek
    return ""; // Zwróć pusty string, jeśli pętla się zakończyła inaczej
}

#endif