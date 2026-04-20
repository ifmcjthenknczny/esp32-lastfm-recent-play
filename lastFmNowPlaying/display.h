#ifndef DISPLAY_H
#define DISPLAY_H

#include <ArduinoJson.h>

/** Initialize TFT (LovyanGFX), rotation, font, and show boot info. */
void displayInit();

/** Clear screen, draw album cover, draw all text, and play icon. */
void displayUpdateAll(const JsonObject& track, const char* albumCoverUrl, bool isPlaying);

void displayUpdateTrackNameOnly(const JsonObject& track);

void displayUpdatePlayIconOnly(bool isPlaying);

/** Show "No recent tracks" message. */
void displayShowNoTracks();

/** Show "WiFi reconnecting" message. */
void displayShowWifiReconnecting();

/** Show "Getting Last.fm data..." and clear screen. */
void displayShowFetching();

#endif
