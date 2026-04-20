#include "LastFmApp.h"
#include "apiConfig.h"
#include "fetch.h"
#include "display.h"
#include "TrackFields.h"
#include "userSettings.h"
#include <ArduinoJson.h>
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
        if (lastDisplayedArtist.length() > 0 || lastDisplayedTrack.length() > 0) {
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
    return static_cast<int>(256.f * brightnessPercent / 100.f - 1.f);
}

void manageDisplayState(bool isPlaying, unsigned long elapsed) {
    DisplayState prevState = displayState;

    bool shouldTurnOn = prevState != DisplayState::On && isPlaying;
    bool shouldTurnOff = prevState != DisplayState::Off && !isPlaying && elapsed >= DISPLAY_OFF_MS;
    bool shouldDim = prevState != DisplayState::Dimmed && !isPlaying && elapsed >= DISPLAY_DIM_MS && elapsed < DISPLAY_OFF_MS;

    if (shouldTurnOn) {
        tft.setBrightness(toDisplayBrightness(DISPLAY_BRIGHTNESS_ON));
        displayState = DisplayState::On;
        Serial.println("DISPLAY ON");
    }
    else if (shouldDim) {
        tft.setBrightness(toDisplayBrightness(DISPLAY_BRIGHTNESS_DIM));
        displayState = DisplayState::Dimmed;
        Serial.println("DISPLAY DIMMED");
    }
    else if (shouldTurnOff) {
        tft.setBrightness(toDisplayBrightness(DISPLAY_BRIGHTNESS_OFF));
        displayState = DisplayState::Off;
        Serial.println("DISPLAY OFF");
    }
}

void logTrackFields(const TrackFields& t) {
    Serial.println(String("Now playing: ") + t.artist + " - " + t.song + " - " + t.album);
}

void updateDisplay(const JsonObject& track, bool isPlaying, unsigned long elapsed) {
    const TrackFields t = trackFieldsFromJson(track);
    const bool artistChanged = (t.artist != lastDisplayedArtist);
    const bool trackChanged  = (t.song != lastDisplayedTrack);
    const bool albumChanged  = (t.album != lastDisplayedAlbum);

    const bool shouldRedrawWholeDisplay = (artistChanged || albumChanged) && isPlaying;
    const bool shouldRedrawTrackOnly = trackChanged && isPlaying;
    const bool shouldRedrawPlayIcon = isPlaying || (!isPlaying && elapsed >= PLAYING_ICON_UPDATE_DELAY_MS) || shouldRedrawWholeDisplay;

    if (shouldRedrawWholeDisplay) {
        String coverUrl = getAlbumCoverUrl(track);
        Serial.println("coverUrl: " + coverUrl);
        displayUpdateAll(track, coverUrl.c_str(), isPlaying);
        logTrackFields(t);
    } else if (shouldRedrawTrackOnly) {
        displayUpdateTrackNameOnly(track);
        logTrackFields(t);
    } 
    
    if (shouldRedrawPlayIcon) {
        displayUpdatePlayIconOnly(isPlaying);
    }

    lastDisplayedArtist = t.artist;
    lastDisplayedTrack  = t.song;
    lastDisplayedAlbum  = t.album;
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
    Serial.println("elapsed: " + String(elapsed) + " ms");
    Serial.println("lastPlayingTime: " + String(lastPlayingTime));
    Serial.println("now: " + String(now));
    manageDisplayState(isPlaying, elapsed);

    if (displayState != DisplayState::On) {
        return;
    }
    updateDisplay(track, isPlaying, elapsed);
}
