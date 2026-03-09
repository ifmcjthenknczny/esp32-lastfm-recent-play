#ifndef DISPLAY_H
#define DISPLAY_H

#include <ArduinoJson.h>

/** Initialize TFT (LovyanGFX), rotation, font, and show boot info. */
void displayInit();

/** Clear screen, draw album cover, draw all text, and play icon. */
void displayUpdate(
    const char* artistName,
    const char* songName,
    const char* albumName,
    const char* albumCoverUrl,
    bool isPlaying
);

void displayUpdateTrackNameOnly(const char* songName);

void displayUpdatePlayIcon(bool isPlaying);

/** Show "No recent tracks" message. */
void displayShowNoTracks();

/** Show "WiFi reconnecting" message. */
void displayShowWifiReconnecting();

/** Show "Getting Last.fm data..." and clear screen. */
void displayShowFetching();

/** Trim/replace text for display; returns adjusted string (may truncate with "..."). */
String displayAdjustTrackText(const String& text);

/** Scale factor for album cover from URL (PNG vs JPG sizes). */
float displayAlbumCoverScale(const String& coverUrl);

#endif
