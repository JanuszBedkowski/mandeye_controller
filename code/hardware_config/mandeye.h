#pragma once

#include "hardware_common.h"

#ifdef MANDEYE_HARDWARE_PRO
#include "mandeye-pro.h"
#endif

#ifdef MANDEYE_HARDWARE_STANDARD
#include "mandeye-standard.h"
#endif

#ifdef MANDEYE_HARDWARE_STANDARD_RPI5
#include "mandeye-standard-rpi5.h"
#endif

#ifndef MANDEYE_HARDWARE_CONFIGURED
#error "MANDEYE Hardware were not configured. You need to include a hardware header!"
#endif

