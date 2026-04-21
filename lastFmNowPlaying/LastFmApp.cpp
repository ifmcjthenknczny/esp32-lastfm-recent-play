#include "LastFmApp.h"
#include "apiConfig.h"
#include "fetch.h"
#include "display.h"
#include "Track.h"
#include "userSettings.h"
#include <ArduinoJson.h>
#include "LGFX.h"

extern LGFX tft;

namespace {

enum class DisplayState { On, Dimmed, Off };

Track lastDisplayedTrack;
DisplayState displayState = DisplayState::On;
unsigned long lastPlayingTime = 0;

unsigned long elapsedSinceLastPlayingTime(unsigned long currentTime = 0) {
    if (lastPlayingTime == 0) {
        return 0;
    }
    unsigned long now = (currentTime == 0) ? millis() : currentTime;
    return now - lastPlayingTime;
}

bool fetchRecentTrack(DynamicJsonDocument& doc, JsonObject& outTrack) {
    String url = String("http://") + LASTFM_HOST + getLastFmRecentTracksPath();
    fetchJson(url.c_str(), doc);

    if (doc.isNull()) {
        Serial.println("fetchRecentTrack: doc is null");
        return false;
    }

    JsonObject recenttracks = doc["recenttracks"];
    if (recenttracks.isNull()) {
        Serial.println("fetchRecentTrack: recenttracks is null");
        return false;
    }

    JsonArray trackArray = recenttracks["track"];
    if (trackArray.isNull() || trackArray.size() == 0) {
        if (lastDisplayedTrack.artist.length() > 0 || lastDisplayedTrack.song.length() > 0) {
            displayShowNoTracks();
        }
        Serial.println("fetchRecentTrack: no tracks");
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

void updateLastPlayingTime() {
    lastPlayingTime = millis();
}

int toDisplayBrightness(float brightnessPercent) {
    const int MAX_BRIGHTNESS = 255;
    int result = static_cast<int>(brightnessPercent * (MAX_BRIGHTNESS / 100.0f));

    if (result < 0) {
        return 0;
    }
    
    return result > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : result;
}

DisplayState computeDisplayState(bool isPlaying, unsigned long elapsed) {
    if (isPlaying) {
        return DisplayState::On;
    }
    if (!isPlaying && elapsed >= DISPLAY_OFF_MS) {
        return DisplayState::Off;
    }
    if (!isPlaying && elapsed >= DISPLAY_DIM_MS && elapsed < DISPLAY_OFF_MS) {
        return DisplayState::Dimmed;
    }
    return displayState;
}

void manageDisplayState(bool isPlaying, unsigned long elapsed) {
    DisplayState newState = computeDisplayState(isPlaying, elapsed);
    const bool hasStateChanged = (newState != displayState);

    if (!isPlaying) {
        Serial.println("Not playing. Time elapsed: " + String(elapsed) + " ms");
    }

    if (!hasStateChanged) {
        return;
    }

    if (newState == DisplayState::On) {
        tft.setBrightness(toDisplayBrightness(DISPLAY_BRIGHTNESS_ON));
        Serial.println("DISPLAY ON");
    }
    else if (newState == DisplayState::Dimmed) {
        tft.setBrightness(toDisplayBrightness(DISPLAY_BRIGHTNESS_DIM));
        Serial.println("DISPLAY DIMMED");
    }
    else if (newState == DisplayState::Off) {
        tft.setBrightness(toDisplayBrightness(DISPLAY_BRIGHTNESS_OFF));
        Serial.println("DISPLAY OFF");
    }
    displayState = newState;
}

void logTrack(const Track& t) {
    Serial.println(String("Now playing: ") + t.artist + " - " + t.song + " - " + t.album);
}

void updateDisplay(const JsonObject& track, bool isPlaying, unsigned long elapsed) {
    const Track t = trackFromJson(track);
    const bool artistChanged = (t.artist != lastDisplayedTrack.artist);
    const bool trackChanged  = (t.song != lastDisplayedTrack.song);
    const bool albumChanged  = (t.album != lastDisplayedTrack.album);

    const bool shouldRedrawWholeDisplay = (artistChanged || albumChanged) && isPlaying;
    const bool shouldRedrawTrackOnly = trackChanged && isPlaying;
    const bool shouldRedrawPlayIcon = isPlaying || (!isPlaying && elapsed >= PLAYING_ICON_UPDATE_DELAY_MS) || shouldRedrawWholeDisplay;

    if (shouldRedrawWholeDisplay) {
        String coverUrl = getAlbumCoverUrl(track);
        Serial.println("coverUrl: " + coverUrl);
        displayUpdateAll(track, coverUrl.c_str(), isPlaying);
    } else if (shouldRedrawTrackOnly) {
        displayUpdateTrackNameOnly(track);
    } 

    if (artistChanged || trackChanged || albumChanged) {
        logTrack(t);
    }
    
    if (shouldRedrawPlayIcon) {
        displayUpdatePlayIconOnly(isPlaying);
    }

    lastDisplayedTrack = t;
}

}  // namespace

void lastFmFetchAndDisplay() {
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    JsonObject track;
    if (!fetchRecentTrack(doc, track)) {
        return;
    }

    const bool isPlaying = isTrackNowPlaying(track);

    if (isPlaying) {
        updateLastPlayingTime();
    }
    const unsigned long now = millis();
    const unsigned long elapsed =
        elapsedSinceLastPlayingTime(now);
    manageDisplayState(isPlaying, elapsed);

    if (displayState != DisplayState::On) {
        return;
    }
    updateDisplay(track, isPlaying, elapsed);
}
