#include "display.h"
#include "LGFX.h"
#include "userSettings.h"
#include "apiConfig.h"
#include "config.h"
#include <HTTPClient.h>

extern LGFX tft;

namespace {

const int ELLIPSIS_LEN = 3;
const int MAX_STRING_LENGTH = MAX_CHARS_IN_LINE - ELLIPSIS_LEN;

struct ReplaceRule {
    const char* from;
    const char* to;
};

const ReplaceRule REPLACE_RULES[] = {
    {"\u2019", "'"},   // right single quote -> apostrophe
    {"\u201C", "\""},  // left double quote
    {"\u201D", "\""},  // right double quote
    {"\u2026", "..."}, // ellipsis
    {"\u2013", "-"},   // en dash
    {"\u2014", "-"},   // em dash
};

}  // namespace

String displayAdjustTrackText(const String& text) {
    String out = text;
    for (const auto& rule : REPLACE_RULES) {
        out.replace(rule.from, rule.to);
    }
    if (out.length() > (unsigned)MAX_CHARS_IN_LINE) {
        out = out.substring(0, MAX_STRING_LENGTH);
        out.trim();
        return out + "...";
    }
    return out;
}

float displayAlbumCoverScale(const String& coverUrl) {
    if (coverUrl.startsWith(JPG_CONVERTER_BUCKET_HOST) || coverUrl.endsWith(".png")) {
        return (float)ALBUM_COVER_SIZE_PX / 300.0f;
    }
    if (coverUrl.endsWith(".jpg") || coverUrl.endsWith(".jpeg")) {
        return (float)ALBUM_COVER_SIZE_PX / 250.0f;
    }
    return (float)ALBUM_COVER_SIZE_PX / 300.0f;
}

void displayInit() {
    Serial.println("Initializing TFT with LovyanGFX...");
    tft.init();
    tft.setRotation(1);
    tft.clear(TFT_BLACK);
    tft.setFont(&myExtendedFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1.25f);
    tft.setCursor(0, 0);
    tft.println("TFT Initialized (LovyanGFX).");
    tft.println("CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
    tft.println("Free RAM: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.println("TFT basics set up.");
}

static void drawAlbumCover(const String& coverUrl) {
    const float scale = displayAlbumCoverScale(coverUrl);
    if (coverUrl.endsWith(".jpg") || coverUrl.endsWith(".jpeg") || coverUrl.startsWith(JPG_CONVERTER_BUCKET_HOST)) {
        tft.drawJpgUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, ALBUM_PADDING_Y_PX, 0, 0, 0, 0, scale, scale);
    } else if (coverUrl.endsWith(".png")) {
        tft.drawPngUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, ALBUM_PADDING_Y_PX, 0, 0, 0, 0, scale, scale);
    }
}

static void drawLabeledLine(const char* label, const String& info, uint16_t labelColor) {
    String trimmed = displayAdjustTrackText(info);
    tft.setFont(&fonts::Font0);
    tft.setTextColor(labelColor, TFT_BLACK);
    tft.setTextSize(TRACK_INFO_LABEL_TEXT_SIZE);
    tft.println(String(label) + ":");
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    tft.setFont(&myExtendedFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(TRACK_INFO_TEXT_SIZE);
    tft.println(trimmed);
    tft.setTextSize(TRACK_INFO_SPACE_SIZE);
    tft.println();
}

static void drawTrackInfo(const char* artist, const char* song, const char* album) {
    tft.setCursor(TEXT_LEFT_PADDING_PX, TEXT_START_HEIGHT_PX);
    drawLabeledLine("Artist", String(artist), TFT_RED);
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    drawLabeledLine("Track", String(song), TFT_GOLD);
    if (album && strlen(album) > 0) {
        tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
        drawLabeledLine("Album", String(album), TFT_CYAN);
    }
}

static void drawPlayIcon(bool isPlaying) {
    const int x = SCREEN_WIDTH_PX - PLAYICON_PX - PLAYICON_PADDING_PX;
    const int y = PLAYICON_PADDING_PX;
    if (isPlaying) {
        tft.drawPngUrl(PLAYICON_URL, x, y);
    } else {
        tft.fillRect(x, y, PLAYICON_PX, PLAYICON_PX, TFT_BLACK);
    }
}

void displayUpdate(const char* artistName, const char* songName, const char* albumName,
                   const char* albumCoverUrl, bool isPlaying) {
    tft.startWrite();
    tft.fillScreen(TFT_BLACK);
    drawAlbumCover(String(albumCoverUrl));
    drawTrackInfo(artistName, songName, albumName);
    drawPlayIcon(isPlaying);
    tft.endWrite();
}

void displayUpdatePlayIcon(bool isPlaying) {
    drawPlayIcon(isPlaying);
}

void displayShowNoTracks() {
    tft.fillScreen(TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1.25f);
    tft.setCursor(0, 0);
    tft.println("No recent tracks found.");
}

void displayShowWifiReconnecting() {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1.0f);
    tft.setCursor(0, 0);
    tft.println("WiFi Lost! Reconnecting...");
}

void displayShowFetching() {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.println("Getting Last.fm data...");
}
