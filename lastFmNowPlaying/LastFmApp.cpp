#include "LastFmApp.h"
#include "apiConfig.h"
#include "fetch.h"
#include "display.h"
#include "userSettings.h"
#include <ArduinoJson.h>
#include <time.h>
#include "LGFX.h"

extern LGFX tft;

namespace {

String lastDisplayedArtist;
String lastDisplayedTrack;
bool displayActive = true;
unsigned long lastActivityTime = 0;

bool fetchRecentTrack(JsonObject& outTrack) {
    String url = String("http://") + LASTFM_HOST + getLastFmRecentTracksPath();
    DynamicJsonDocument doc = fetchJson(url.c_str());
    if (doc.isNull()) return false;

    JsonObject recenttracks = doc["recenttracks"];
    if (recenttracks.isNull()) return false;

    JsonArray trackArray = recenttracks["track"];
    if (trackArray.isNull() || trackArray.size() == 0) {
        if (lastDisplayedArtist.length() > 0 || lastDisplayedTrack.length() > 0) {
            displayShowNoTracks();
        }
        return false;
    }
    outTrack = trackArray[0];
    return true;
}

bool isTrackNowPlaying(const JsonObject& track) {
    return track.containsKey("@attr") &&
           track["@attr"].containsKey("nowplaying") &&
           track["@attr"]["nowplaying"].as<String>() == "true";
}

void updateLastActivityTime(const JsonObject& track, bool isPlaying) {
    if (isPlaying) {
        lastActivityTime = (unsigned long)time(nullptr);
    } else if (track.containsKey("date") && track["date"].is<JsonObject>() && track["date"].containsKey("uts")) {
        lastActivityTime = track["date"]["uts"].as<unsigned long>();
    } else {
        lastActivityTime = (unsigned long)time(nullptr);
    }
}

void manageDisplayActive(bool isPlaying) {
    if (isPlaying && !displayActive) {
        displayActive = true;
        lastDisplayedArtist = "";
        lastDisplayedTrack = "";
        Serial.println("Display ON");
        return;
    }
    if (!isPlaying && displayActive) {
        unsigned long elapsed = (unsigned long)time(nullptr) - lastActivityTime;
        if (elapsed > DISPLAY_OFF_MS / 1000) {
            displayActive = false;
            lastDisplayedArtist = "";
            lastDisplayedTrack = "";
            tft.clear(TFT_BLACK);
            Serial.println("Display OFF");
        }
    }
}

bool shouldRedrawDisplay(const String& artist, const String& track) {
    if (!displayActive) return false;
    if (artist == lastDisplayedArtist && track == lastDisplayedTrack) return false;
    return true;
}

}  // namespace

void lastFmFetchAndDisplay() {
    JsonObject track;
    if (!fetchRecentTrack(track)) return;

    bool isPlaying = isTrackNowPlaying(track);
    updateLastActivityTime(track, isPlaying);
    manageDisplayActive(isPlaying);

    String artistName = track["artist"]["#text"] | "Unknown Artist";
    String songName  = track["name"] | "Unknown Track";

    if (!shouldRedrawDisplay(artistName, songName)) {
        displayUpdatePlayIcon(isPlaying);
        return;
    }

    String albumName = track["album"]["#text"] | "Unknown Album";
    String coverUrl  = resolveAlbumCoverUrl(track);

    displayUpdate(
        artistName.c_str(),
        songName.c_str(),
        albumName.c_str(),
        coverUrl.c_str(),
        isPlaying
    );

    lastDisplayedArtist = artistName;
    lastDisplayedTrack  = songName;
}
