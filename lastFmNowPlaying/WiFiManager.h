#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "ConfigDecl.h"

namespace wifi {

/** Connect to WiFi (blocking). On failure, shows error on display and halts. */
void connect();

/** Returns true if WiFi is connected. */
bool isConnected();

/** Try once to reconnect. Call from loop() when isConnected() is false. */
bool tryReconnect();

}  // namespace wifi

#endif
