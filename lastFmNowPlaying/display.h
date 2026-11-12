#ifndef DISPLAY_H
#define DISPLAY_H

#include <ArduinoJson.h>

namespace display {

/** Initialize TFT (LovyanGFX), rotation, font, and show boot info. */
void init();

/** Clear screen, draw album cover, draw all text, and play icon. */
void updateAll(const JsonObject& track, const char* albumCoverUrl, bool isPlaying);

void updateTrackNameOnly(const JsonObject& track);

void updatePlayIcon(bool isPlaying);

/** Show "No recent tracks" message. */
void showNoTracks();

/** Show "WiFi reconnecting" message. */
void showWifiReconnecting();

/** Show "Fetching Last.fm data..." and clear screen. */
void showFetching();

} // namespace display

#endif
