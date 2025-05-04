#ifndef API_CONFIG_H
#define API_CONFIG_H

// --- Last.fm API Details ---
const char* LASTFM_HOST = "ws.audioscrobbler.com";
const int LASTFM_PORT = 80;
String LASTFM_PATH = "/2.0/?method=user.getrecenttracks&user=" + String(LASTFM_USERNAME) + "&api_key=" + String(LASTFM_APIKEY) + "&format=json&limit=1";

const char* COVERALBUM_HOST = "coverartarchive.org";
const int COVERALBUM_PORT = 443;

String coverAlbumPath(String mbid) {
    return "/release/" + String(mbid) + "?fmt=json";
}

const size_t JSON_BUFFER_SIZE = 2048;

const String PLAYICON_URL = "https://i.ibb.co/h1RVNJT6/118620-play-icon.png";

#endif