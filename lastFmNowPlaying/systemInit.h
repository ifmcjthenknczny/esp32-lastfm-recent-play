#ifndef INIT_H
#define INIT_H

#include "ConfigDecl.h"

namespace systemInit {

/** Initialize serial at 115200, wait up to 2 s for connection. */
void initSerial();

/** Sync time via NTP (CET timezone). Blocks until time is synced. */
void syncTime();

}  // namespace systemInit

#endif
