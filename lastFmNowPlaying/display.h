#include "userSettings.h"

const int ELLIPSIS_LEN = 3;
const int MAX_STRING_LENGTH = MAX_CHARS_IN_LINE - ELLIPSIS_LEN;

String adjustTrackInfo(String label) {
    label.replace("’", "'");
    label.replace("“", "\"");
    label.replace("”", "\"");
    label.replace("…", "...");
    label.replace("–", "-");
    label.replace("—", "-");
    if (label.length() > MAX_CHARS_IN_LINE) {
        return label.substring(0, MAX_STRING_LENGTH) + "...";
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