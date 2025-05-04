#ifndef API_CONFIG_H
#define API_CONFIG_H

// --- Last.fm API Details ---
const char* LASTFM_HOST = "ws.audioscrobbler.com";
const int LASTFM_PORT = 80; // Standard HTTP port
String LASTFM_PATH = "/2.0/?method=user.getrecenttracks&user=" + String(LASTFM_USERNAME) + "&api_key=" + String(LASTFM_APIKEY) + "&format=json&limit=1";

const char* COVERALBUM_HOST = "coverartarchive.org";

String coverAlbumPath(String mbid) {
    return "/release/" + String(mbid) + "?fmt=json";
}

#endif