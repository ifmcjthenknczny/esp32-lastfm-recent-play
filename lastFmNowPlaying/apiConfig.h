#ifndef API_CONFIG_H
#define API_CONFIG_H

#include "config.h"

// --- Last.fm API ---
const char* LASTFM_HOST = "ws.audioscrobbler.com";
const int   LASTFM_PORT = 80;

/** Builds Last.fm recent tracks API path (avoids global String). */
inline String getLastFmRecentTracksPath() {
    return String("/2.0/?method=user.getrecenttracks&user=") + LASTFM_USERNAME +
           "&api_key=" + LASTFM_APIKEY + "&format=json&limit=1";
}

// --- Cover Art Archive ---
const char* COVERALBUM_HOST = "coverartarchive.org";
const int   COVERALBUM_PORT = 443;

inline String coverAlbumPath(const String& mbid) {
    return String("/release/") + mbid + "?fmt=json";
}

// --- Buffers & assets ---
const size_t JSON_BUFFER_SIZE = 2048;
const int    PLAYICON_PX      = 24;
const char*  PLAYICON_URL     = "https://i.ibb.co/204LYysN/playnow-small.png";

#endif
