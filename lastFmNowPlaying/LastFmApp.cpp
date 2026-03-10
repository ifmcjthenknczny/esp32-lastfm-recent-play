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
String lastDisplayedAlbum;
bool displayActive = true;
unsigned long lastPlayingTime = 0;

bool fetchRecentTrack(DynamicJsonDocument& doc, JsonObject& outTrack) {
    String url = String("http://") + LASTFM_HOST + getLastFmRecentTracksPath();
    fetchJson(url.c_str(), doc);
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

void updateLastPlayingTime(const JsonObject& track, bool isPlaying) {
    if (isPlaying) {
        lastPlayingTime = (unsigned long)time(nullptr);
    }
}

void manageDisplayActive(bool isPlaying) {
    if (isPlaying && !displayActive) {
        displayActive = true;
        lastDisplayedArtist = "";
        lastDisplayedTrack = "";
        tft.setBrightness(255);
        Serial.println("Display ON");
    }
    if (!isPlaying && displayActive) {
        unsigned long elapsed = (unsigned long)time(nullptr) - lastPlayingTime;

        if (elapsed > DISPLAY_OFF_MS / 1000) {
            tft.setBrightness(0);
            displayActive = false;
            Serial.println("Display OFF");
        }
        else if (elapsed > DISPLAY_DIM_MS / 1000) {
            tft.setBrightness(96);
            Serial.println("Display DIM");
        }
    }
}
}  // namespace

void lastFmFetchAndDisplay() {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    JsonObject track;
    if (!fetchRecentTrack(doc, track)) return;

    bool isPlaying = isTrackNowPlaying(track);
    updateLastPlayingTime(track, isPlaying);
    manageDisplayActive(isPlaying);

    if (!displayActive) return;

    String artistName = track["artist"]["#text"] | "Unknown Artist";
    String songName   = track["name"] | "Unknown Track";
    String albumName  = track["album"]["#text"] | "Unknown Album";
    String coverUrl   = getAlbumCoverUrl(track);

    bool artistChanged = (artistName != lastDisplayedArtist);
    bool trackChanged  = (songName != lastDisplayedTrack);
    bool albumChanged  = (albumName != lastDisplayedAlbum);

    bool shouldRedrawWholeDisplay = artistChanged || albumChanged;
    bool shouldRedrawTrackOnly = !artistChanged && !albumChanged && trackChanged;

    if (shouldRedrawWholeDisplay) {
        displayUpdate(artistName.c_str(), songName.c_str(), albumName.c_str(),
                      coverUrl.c_str(), isPlaying);
    } else if (shouldRedrawTrackOnly) {
        displayUpdateTrackNameOnly(songName.c_str());
    } else {
        displayUpdatePlayIcon(isPlaying);
    }

    lastDisplayedArtist   = artistName;
    lastDisplayedTrack    = songName;
    lastDisplayedAlbum    = albumName;
}
