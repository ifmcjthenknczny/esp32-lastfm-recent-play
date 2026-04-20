#ifndef TRACK_FIELDS_H
#define TRACK_FIELDS_H

#include <ArduinoJson.h>
#include <WString.h>

struct TrackFields {
    String artist;
    String song;
    String album;
};

inline TrackFields trackFieldsFromJson(const JsonObject& track) {
    return {track["artist"]["#text"] | "Unknown",
            track["name"] | "Unknown",
            track["album"]["#text"] | "Unknown"};
}

#endif
