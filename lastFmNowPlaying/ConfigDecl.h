#ifndef CONFIG_DECL_H
#define CONFIG_DECL_H

/* Declarations only. Definitions are in config.h, which must be included
   exactly once (in the main .ino). Use this header in all .cpp and other .h
   so there are no multiple-definition linker errors. */

extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const char* LASTFM_APIKEY;
extern const char* LASTFM_USERNAME;
extern const char* JPG_CONVERTER_URL;
extern const char* JPG_CONVERTER_URL_API_KEY;
extern const char* JPG_CONVERTER_BUCKET_HOST;

#endif
