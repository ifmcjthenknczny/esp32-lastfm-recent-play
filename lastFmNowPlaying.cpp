#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include <WiFi.h>
#include "config.h"

String wifiSSID;
String wifiPassword;
String apiKey;
String username;
const String LASTFM_API = "http://ws.audioscrobbler.com/2.0/?method=user.getrecenttracks&user=" + USERNAME + "&api_key=" + API_KEY + "&format=json";

// Obiekt TFT do obsługi ekranu
TFT_eSPI tft = TFT_eSPI();  

// Zmienna do pobierania URL okładki
String albumCoverUrl = "";

void setup() {
  // Inicjalizacja ekranu
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  // Połączenie z Wi-Fi
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Łączenie z Wi-Fi...");
  }
  
  Serial.println("Połączono z Wi-Fi!");

  // Pobieranie danych z Last.fm
  getNowPlaying();
}

void loop() {
  // W pętli nic się nie dzieje, bo dane są pobierane raz podczas setup()
  delay(10000); // Możesz ustawić czas na odświeżanie
}

void getNowPlaying() {
  HTTPClient http;
  http.begin(LASTFM_API);
  int httpCode = http.GET();  // Wykonanie żądania GET

  if (httpCode == 200) {  // Jeśli odpowiedź jest OK (200)
    String payload = http.getString();
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    // Wyszukiwanie aktualnie odtwarzanego utworu
    JsonObject track = doc["recenttracks"]["track"][0];
    String songName = track["name"];
    String albumName = track["album"]["#text"];
    String artistName = track["artist"]["#text"];
    albumCoverUrl = track["image"][1]["#text"];  // Pobranie URL okładki albumu

    // Wyświetlanie informacji na ekranie
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Wyświetlanie okładki albumu
    if (albumCoverUrl != "") {
      // Pobieranie obrazu z URL (zrób to ręcznie z serwera)
      displayAlbumCover(albumCoverUrl);
    }
    
    // Wyświetlanie tekstów
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.println("Wykonawca:");
    tft.setTextSize(2);
    tft.println(artistName);  // Nazwa wykonawcy

    tft.setTextSize(1);
    tft.println("Utwór:");
    tft.setTextSize(2);  
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Bold
    tft.println(songName);  // Nazwa utworu

    tft.setTextSize(1);
    tft.println("Album:");
    tft.setTextSize(1);  // Regular
    tft.setTextColor(TFT_WHITE, TFT_BLACK); 
    tft.println(albumName); // Nazwa albumu
  } else {
    Serial.println("Błąd przy pobieraniu danych.");
  }

  http.end();  // Zakończenie połączenia HTTP
}

void displayAlbumCover(String coverUrl) {
  // Zainicjuj HTTPClient dla pobierania obrazu
  HTTPClient http;
  http.begin(coverUrl);
  int httpCode = http.GET();

  if (httpCode == 200) {
    WiFiClient * stream = http.getStreamPtr();
    int bufferSize = 1024;
    uint8_t buffer[bufferSize];
    
    // Odczytuj obrazek w częściach
    int bytesRead = stream->read(buffer, bufferSize);
    
    // Tutaj możesz przetworzyć obrazek na TFT, np. za pomocą SPIFFS (System plików w ESP32)
    // Dalsze kroki będą zależne od formatu okładki (np. JPEG, PNG) i przetwarzania go na wyświetlaczu
  } else {
    Serial.println("Błąd podczas pobierania okładki.");
  }
  http.end();
}
