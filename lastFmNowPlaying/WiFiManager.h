#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "config.h"

namespace wifi {

/** Initialize serial at 115200, wait up to 2 s for connection. */
void initSerial();

/** Connect to WiFi (blocking). On failure, shows error on display and halts. */
void connect();

/** Sync time via NTP (CET timezone). Blocks until time is synced. */
void syncTime();

/** Returns true if WiFi is connected. */
bool isConnected();

/** Try once to reconnect. Call from loop() when isConnected() is false. */
bool tryReconnect();

}  // namespace wifi

#endif
