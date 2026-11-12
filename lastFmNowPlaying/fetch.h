#ifndef FETCH_H
#define FETCH_H

#include <ArduinoJson.h>

namespace fetch {

/** Fetch URL and parse response as JSON into outDoc. Clears/fills outDoc; no
 * extra allocation. */
void json(const char* url, DynamicJsonDocument& outDoc);

/** Resolve album cover image URL from Last.fm track object (may use CAA or JPG
 * converter). */
String albumCoverUrl(JsonObject track);

}  // namespace fetch

#endif
