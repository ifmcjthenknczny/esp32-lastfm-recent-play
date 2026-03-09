#ifndef API_CONFIG_H
#define API_CONFIG_H

#include <Arduino.h>
#include "ConfigDecl.h"

// --- Last.fm API ---
static const char* LASTFM_HOST = "ws.audioscrobbler.com";
static const int   LASTFM_PORT = 80;

/** Builds Last.fm recent tracks API path (avoids global String). */
inline String getLastFmRecentTracksPath() {
    return String("/2.0/?method=user.getrecenttracks&user=") + LASTFM_USERNAME +
           "&api_key=" + LASTFM_APIKEY + "&format=json&limit=1";
}

// --- Cover Art Archive ---
static const char* COVERALBUM_HOST = "coverartarchive.org";
static const int   COVERALBUM_PORT = 443;

inline String coverAlbumPath(const String& mbid) {
    return String("/release/") + mbid + "?fmt=json";
}

// --- Buffers & assets ---
static const size_t JSON_BUFFER_SIZE = 2048;
static const int    PLAYICON_PX      = 24;
static const char* PLAYICON_URL      = "https://i.ibb.co/204LYysN/playnow-small.png";

#endif
