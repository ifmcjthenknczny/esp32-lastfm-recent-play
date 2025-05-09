#ifndef USER_SETTINGS_H
#define USER_SETTINGS_H

const int REFRESH_MS = 3000;
const unsigned long DISPLAY_OFF_MS = 30000;
const int TEXT_START_HEIGHT_PX = 136;
const int PLAYICON_PADDING_PX = 5;
const int TEXT_LEFT_PADDING_PX = 10;
const int ALBUM_COVER_SIZE_PX = 240; // tft.width();
const bool IS_ALBUM_CENTERED = true;
int ALBUM_PADDING_X_PX = IS_ALBUM_CENTERED ? (320 - ALBUM_COVER_SIZE_PX) / 2 : 0;
int ALBUM_PADDING_Y_PX = IS_ALBUM_CENTERED ? (240 - ALBUM_COVER_SIZE_PX) /2 : 0;
const float TRACK_INFO_TEXT_SIZE = 1.25;
const float TRACK_INFO_LABEL_TEXT_SIZE = 1;
const float TRACK_INFO_SPACE_SIZE = 0.5;

#endif