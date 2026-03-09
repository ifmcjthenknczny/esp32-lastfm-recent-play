#ifndef CONFIG_H
#define CONFIG_H

// --- WiFi ---
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// --- Last.fm API (get key: https://www.last.fm/api/account/create) ---
const char* LASTFM_APIKEY  = "YOUR_LASTFM_API_KEY";
const char* LASTFM_USERNAME = "YOUR_LASTFM_USERNAME";

// --- Optional: JPG converter (progressive → baseline) and S3 bucket for cached covers ---
// Leave empty if you don't use custom converter (PNG covers from Last.fm will still work).
const char* JPG_CONVERTER_URL         = "";
const char* JPG_CONVERTER_URL_API_KEY = "";
const char* JPG_CONVERTER_BUCKET_HOST = "";

#endif
