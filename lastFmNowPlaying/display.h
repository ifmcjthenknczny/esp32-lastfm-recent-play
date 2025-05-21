#include "userSettings.h"

const int ELLIPSIS_LEN = 3;
const int MAX_STRING_LENGTH = MAX_CHARS_IN_LINE - ELLIPSIS_LEN;

struct ReplaceRule {
    const char* from;
    const char* to;
};

const ReplaceRule REPLACE_RULES[] = {
    {"’", "'"},
    {"“", "\""},
    {"”", "\""},
    {"…", "..."},
    {"–", "-"},
    {"—", "-"},
};

String adjustTrackInfo(String label) {
    for (const auto& rule : REPLACE_RULES) {
        label.replace(rule.from, rule.to);
    }
    if (label.length() > MAX_CHARS_IN_LINE) {
        label = label.substring(0, MAX_STRING_LENGTH);
        label.trim();
        return label + "...";
    }

    return label;
}

float calculateAlbumCoverScale (String coverUrl) {
    int expectedCoverSize = 300;
    if (coverUrl.startsWith("https://playing-now-album-covers.s3.eu-central-1.amazonaws.com") || coverUrl.endsWith(".png")) {
        expectedCoverSize = 300;
    } else if (coverUrl.endsWith(".jpg") || coverUrl.endsWith(".jpeg")) {
        expectedCoverSize = 250;
    }
    return ALBUM_COVER_SIZE_PX / expectedCoverSize;
}