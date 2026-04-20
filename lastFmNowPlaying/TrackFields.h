#ifndef TRACK_FIELDS_H
#define TRACK_FIELDS_H

#include <ArduinoJson.h>
#include <WString.h>

struct Track {
    String artist;
    String song;
    String album;
};

inline Track trackFromJson(const JsonObject& track) {
    return {track["artist"]["#text"] | "Unknown",
            track["name"] | "Unknown",
            track["album"]["#text"] | "Unknown"};
}

#endif
