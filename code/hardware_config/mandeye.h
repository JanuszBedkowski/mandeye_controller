#pragma once

#include "compilation_constants.h"
#include "hardware_common.h"

#ifndef MANDEYE_HARDWARE_HEADER
#error "MANDEYE_HARDWARE_HEADER definition were not configured. You need to include a hardware header!"
#endif
#ifdef MANDEYE_HARDWARE_HEADER
#pragma message("Including hardware header " MANDEYE_HARDWARE_HEADER)
#include MANDEYE_HARDWARE_HEADER
#endif



#ifndef MANDEYE_HARDWARE_CONFIGURED
#error "MANDEYE Hardware were not configured. You need to include a hardware header!"
#endif

