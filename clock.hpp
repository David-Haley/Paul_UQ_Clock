// Software to test individula LEDs on Paul's clock display board 20 LEDs.
// Note this has been built for the PI Pico W
// Author    : David Haley
// Created   : 31/09/2025
// Last Edit : 03/09/2026

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// Guards all RP2040 RTC hardware accesses (rtc_get_datetime / rtc_set_datetime).
// Shared with ntp.cpp, which sets the RTC from a different core once an hour;
// without this, Clock_Set could read the RTC mid-update.
extern SemaphoreHandle_t RTC_Mutex;

// Aux LED indices, inclusive, per aux_led.json schema.
#define Aux_LED_First 12
#define Aux_LED_Last 18
#define Aux_LED_Count (Aux_LED_Last - Aux_LED_First + 1)

// Guards Aux_LED_State (colour/flash for LEDs Aux_LED_First .. Aux_LED_Last).
// Shared with aux_led.cpp, which writes it from parsed MQTT/JSON messages;
// Clock_Set reads it every call.
extern SemaphoreHandle_t Aux_LED_Mutex;

// Guards Current_Brightness (clock.cpp) -- the LED Brightness (2..255, per
// Addressable_LED::Set_One) derived from the VEML7700 ambient light sensor.
// Shared with main.c's heartbeat_task, which writes it via
// Clock_Set_Brightness once per successful VEML7700_Read_ALS; Clock_Set
// reads it under this mutex once per call.
extern SemaphoreHandle_t Brightness_Mutex;

void Clock_Init (void);

void Clock_Set (void);

// Replaces the whole aux LED bank (indices Aux_LED_First .. Aux_LED_Last) in
// one atomic step, so a message that omits an LED reliably turns it black
// rather than leaving a stale colour from a previous message. Colour and
// Flash are indexed 0 .. Aux_LED_Count - 1, corresponding to LED
// Aux_LED_First .. Aux_LED_Last.
void Clock_Set_Aux_LEDs (const uint32_t Colour [Aux_LED_Count],
  const bool Flash [Aux_LED_Count]);

// Sets the LED Brightness (2 = dimmest, 255 = brightest) used by every
// Set_One call in Clock_Set, in one atomic step under Brightness_Mutex.
// Called by main.c's heartbeat_task after each successful
// VEML7700_Read_ALS; a failed read leaves the last-set Brightness in effect.
void Clock_Set_Brightness (unsigned char Brightness);

#ifdef __cplusplus
}
#endif
