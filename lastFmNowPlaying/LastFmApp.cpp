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

enum class DisplayState { On, Dimmed, Off };

const char* displayStateStr(DisplayState s) {
    switch (s) {
        case DisplayState::On:    return "on";
        case DisplayState::Dimmed: return "dimmed";
        case DisplayState::Off:   return "off";
    }
    return "on";
}

String lastDisplayedArtist;
String lastDisplayedTrack;
String lastDisplayedAlbum;
DisplayState displayState = DisplayState::On;
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

void manageDisplayState(bool isPlaying) {
    DisplayState prevState = displayState;

    if (isPlaying && displayState != DisplayState::On) {
        tft.setBrightness(255);
        displayState = DisplayState::On;
        Serial.println("DISPLAY ON");
    }
    else if (!isPlaying) {
        unsigned long elapsed = (unsigned long)time(nullptr) - lastPlayingTime;

        if (elapsed > DISPLAY_OFF_MS / 1000 && displayState != DisplayState::Off) {
            tft.setBrightness(0);
            displayState = DisplayState::Off;
            Serial.println("DISPLAY OFF");
        }
        else if (elapsed > DISPLAY_DIM_MS / 1000 && elapsed <= DISPLAY_OFF_MS / 1000 && displayState != DisplayState::Dimmed) {
            tft.setBrightness(64);
            displayState = DisplayState::Dimmed;
            Serial.println("DISPLAY DIMMED");
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
    manageDisplayState(isPlaying);

    if (displayState == DisplayState::Off) return;

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
