#include "display.h"
#include "Track.h"
#include "LGFX.h"
#include "userSettings.h"
#include "apiConfig.h"
#include "ConfigDecl.h"
#include <HTTPClient.h>

extern LGFX tft;

static LGFX_Sprite titleBgSprite(&tft);

namespace {

static const int ELLIPSIS_LENGTH = 3;
static const int MAX_STRING_LENGTH = MAX_CHARS_IN_LINE - ELLIPSIS_LENGTH;

const int LABEL_LINE_PX  = 8;
const int VALUE_LINE_PX  = 20;
const int SPACE_LINE_PX  = 8;
const int TRACK_VALUE_Y = TEXT_START_HEIGHT_PX + LABEL_LINE_PX + VALUE_LINE_PX + SPACE_LINE_PX + LABEL_LINE_PX;

unsigned int SCREEN_WIDTH_PX = 0;
unsigned int SCREEN_HEIGHT_PX = 0;
unsigned int ALBUM_COVER_SIZE_PX = 0;
unsigned int ALBUM_PADDING_X_PX = 0;
unsigned int ALBUM_PADDING_Y_PX = 0;

struct ReplaceRule {
    const char* from;
    const char* to;
};

static const ReplaceRule REPLACE_RULES[] = {
    {"\u2019", "'"},   // right single quote -> apostrophe
    {"\u201C", "\""},  // left double quote
    {"\u201D", "\""},  // right double quote
    {"\u2026", "..."}, // ellipsis
    {"\u2013", "-"},   // en dash
    {"\u2014", "-"},   // em dash
};

}  // namespace

static String adjustTrackText(const String& text) {
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

static float albumCoverScale(const String& coverUrl) {
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

    SCREEN_WIDTH_PX = tft.width();
    SCREEN_HEIGHT_PX = tft.height();

    Serial.println("Display size: " + String(SCREEN_WIDTH_PX) + "x" + String(SCREEN_HEIGHT_PX) + " px");

    ALBUM_COVER_SIZE_PX = SCREEN_HEIGHT_PX;

    ALBUM_PADDING_X_PX = IS_ALBUM_CENTERED ? (SCREEN_WIDTH_PX - ALBUM_COVER_SIZE_PX) / 2 : 0;
    ALBUM_PADDING_Y_PX = IS_ALBUM_CENTERED ? (SCREEN_HEIGHT_PX - ALBUM_COVER_SIZE_PX) / 2 : 0;

    Serial.println("Album padding x: " + String(ALBUM_PADDING_X_PX) + " px");
    Serial.println("Album padding y: " + String(ALBUM_PADDING_Y_PX) + " px");

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
    const float scale = albumCoverScale(coverUrl);
    if (coverUrl.endsWith(".jpg") || coverUrl.endsWith(".jpeg") || coverUrl.startsWith(JPG_CONVERTER_BUCKET_HOST)) {
        tft.drawJpgUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, ALBUM_PADDING_Y_PX, 0, 0, 0, 0, scale, scale);
    } else if (coverUrl.endsWith(".png")) {
        tft.drawPngUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, ALBUM_PADDING_Y_PX, 0, 0, 0, 0, scale, scale);
    }
}

static void drawLabeledLine(const char* label, const String& info, uint16_t labelColor) {
    String trimmed = adjustTrackText(info);
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

static void drawTrackInfo(const Track& t) {
    tft.setCursor(TEXT_LEFT_PADDING_PX, TEXT_START_HEIGHT_PX);
    drawLabeledLine("Artist", t.artist, TFT_RED);
    tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
    drawLabeledLine("Track", t.song, TFT_GOLD);
    if (t.album.length() > 0) {
        tft.setCursor(TEXT_LEFT_PADDING_PX, tft.getCursorY());
        drawLabeledLine("Album", t.album, TFT_CYAN);
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

static void populateTitleBgSprite(const String& coverUrl) {
    tft.setFont(&myExtendedFont);
    tft.setTextSize(TRACK_INFO_TEXT_SIZE);
    const int h = tft.fontHeight();

    titleBgSprite.deleteSprite();
    if (!titleBgSprite.createSprite(SCREEN_WIDTH_PX, h)) {
        return;
    }
    titleBgSprite.fillSprite(TFT_BLACK);

    const float scale = albumCoverScale(coverUrl);
    const int spriteY = ALBUM_PADDING_Y_PX - TRACK_VALUE_Y;
    if (coverUrl.endsWith(".jpg") || coverUrl.endsWith(".jpeg") || coverUrl.startsWith(JPG_CONVERTER_BUCKET_HOST)) {
        titleBgSprite.drawJpgUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, spriteY, 0, 0, 0, 0, scale, scale);
    } else if (coverUrl.endsWith(".png")) {
        titleBgSprite.drawPngUrl(coverUrl.c_str(), ALBUM_PADDING_X_PX, spriteY, 0, 0, 0, 0, scale, scale);
    }
}

void displayUpdateAll(const JsonObject& track, const char* albumCoverUrl, bool isPlaying) {
    const Track t = trackFromJson(track);
    tft.startWrite();
    tft.fillScreen(TFT_BLACK);
    drawAlbumCover(String(albumCoverUrl));
    drawTrackInfo(t);
    tft.endWrite();

    populateTitleBgSprite(String(albumCoverUrl));
}

void displayUpdateTrackNameOnly(const JsonObject& track) {
    const Track t = trackFromJson(track);
    tft.setFont(&myExtendedFont);
    tft.setTextSize(TRACK_INFO_TEXT_SIZE);

    if (titleBgSprite.width() > 0) {
        titleBgSprite.pushSprite(0, TRACK_VALUE_Y);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
        tft.fillRect(TEXT_LEFT_PADDING_PX, TRACK_VALUE_Y,
                     SCREEN_WIDTH_PX - TEXT_LEFT_PADDING_PX, tft.fontHeight(), TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    tft.setCursor(TEXT_LEFT_PADDING_PX, TRACK_VALUE_Y);
    tft.print(adjustTrackText(t.song));
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
