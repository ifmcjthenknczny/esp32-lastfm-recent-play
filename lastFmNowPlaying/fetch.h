#ifndef FETCH_H
#define FETCH_H

#include <ArduinoJson.h>

/** Fetch URL and parse response as JSON. Returns empty doc on failure. */
DynamicJsonDocument fetchJson(const char* url);

/** Resolve album cover image URL from Last.fm track object (may use CAA or JPG converter). */
String resolveAlbumCoverUrl(JsonObject track);

#endif
