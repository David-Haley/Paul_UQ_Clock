// Software to test individula LEDs on Paul's clock display board 20 LEDs.
// Note this has been built for the PI Pico W
// Author    : David Haley
// Created   : 31/09/2025
// Last Edit : 31/09/2025

#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// Guards all RP2040 RTC hardware accesses (rtc_get_datetime / rtc_set_datetime).
// Shared with ntp.cpp, which sets the RTC from a different core once an hour;
// without this, Clock_Set could read the RTC mid-update.
extern SemaphoreHandle_t RTC_Mutex;

void Clock_Init (void);

void Clock_Set (void);

#ifdef __cplusplus
}
#endif
