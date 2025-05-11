#ifndef USER_SETTINGS_H
#define USER_SETTINGS_H

const int SCREEN_WIDTH_PX = 320;
const int SCREEN_HEIGHT_PX = 240;

const int REFRESH_MS = 5000;
const unsigned long DISPLAY_OFF_MS = 30000;
const int TEXT_START_HEIGHT_PX = 136;
const int PLAYICON_PADDING_PX = 5;
const int TEXT_LEFT_PADDING_PX = 10;
const int ALBUM_COVER_SIZE_PX = 240; // tft.width();
const bool IS_ALBUM_CENTERED = true;
int ALBUM_PADDING_X_PX = IS_ALBUM_CENTERED ? (SCREEN_WIDTH_PX - ALBUM_COVER_SIZE_PX) / 2 : 0;
int ALBUM_PADDING_Y_PX = IS_ALBUM_CENTERED ? (SCREEN_HEIGHT_PX - ALBUM_COVER_SIZE_PX) /2 : 0;
const float TRACK_INFO_TEXT_SIZE = 1.25;
const float TRACK_INFO_LABEL_TEXT_SIZE = 1;
const float TRACK_INFO_SPACE_SIZE = 0.5;
const int MAX_CHARS_IN_LINE = 31;

#endif