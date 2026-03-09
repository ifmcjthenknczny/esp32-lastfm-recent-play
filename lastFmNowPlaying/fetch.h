#ifndef FETCH_H
#define FETCH_H

#include <ArduinoJson.h>

/** Fetch URL and parse response as JSON into outDoc. Clears/fills outDoc; no extra allocation. */
void fetchJson(const char* url, DynamicJsonDocument& outDoc);

/** Resolve album cover image URL from Last.fm track object (may use CAA or JPG converter). */
String getAlbumCoverUrl(JsonObject track);

#endif
