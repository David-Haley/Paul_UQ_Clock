// Software to test individula LEDs on Paul's clock display board 20 LEDs.
// Note this has been built for the PI Pico W
// Author    : David Haley
// Created   : 26/11/2022
// Last Edit : 01/09/2026

// 20250901 : Start of implementation as a clock.
// 20231105: Setting to black moved to start of cycle so that the LED data is
// not overwritten before the DMA process has completed. 

#include "pico/stdlib.h"
#include "clock.hpp"
#include "addressable_led.hpp"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"

const uint Clock_Count = 20; // number of LEDs in string
const uint Clock_Colours = 6;
const uint32_t Colour_Table [Clock_Colours] =
		{
            Addressable_LED :: Red,
            Addressable_LED :: Yellow,
            Addressable_LED :: Green,
            Addressable_LED :: Cyan,
            Addressable_LED :: Blue,
            Addressable_LED :: Magenta
        }; // Colour_Table

Addressable_LED *Clock_String;

const uint Clock_Hours = 12; // number of LEDs used to represent time

SemaphoreHandle_t RTC_Mutex = NULL;

SemaphoreHandle_t Aux_LED_Mutex = NULL;

SemaphoreHandle_t Brightness_Mutex = NULL;

struct Aux_LED_Entry
{
    uint32_t Colour;
    bool Flash;
}; // Aux_LED_Entry

static Aux_LED_Entry Aux_LED_State [Aux_LED_Count];

// Brightness (2..255) applied to every Set_One call in Clock_Set; written by
// Clock_Set_Brightness under Brightness_Mutex, read the same way once per
// Clock_Set call. Starts at 255 (full brightness) because led_task (higher
// priority) runs Clock_Set well before heartbeat_task's first VEML7700
// reading can arrive -- see main.c.
static unsigned char Current_Brightness = 255;

// Inverted once per Clock_Set call (2Hz) to give "flash" aux LEDs a 1Hz cycle.
static bool Flasher = false;

void Clock_Init (void)
{
    // Placeholder start time so the RTC is running (and rtc_get_datetime in
    // Clock_Set has something valid to read) before the first NTP sync (see
    // ntp.cpp) overwrites it with real local time.
    datetime_t Time = {
        .year = 2026,
        .month = 1,
        .day = 1,
        .dotw = 4, // 0 is Sunday, so 4 is Thursday
        .hour = 0,
        .min = 0,
        .sec = 0
    };
    RTC_Mutex = xSemaphoreCreateMutex ();
    rtc_init();
    rtc_set_datetime(&Time);
    sleep_us (64); // delay is required because of slow rtc hardware
    Clock_String =
      new Addressable_LED (Clock_Count, pio0, 0, Addressable_LED :: J1);
    Clock_String->Solid (Addressable_LED :: Black);
    // Created here, before the scheduler can run any lower-priority task
    // (led_task has the highest priority and calls Clock_Init first), so
    // Aux_LED_Mutex always exists by the time aux_led.cpp's task could call
    // Clock_Set_Aux_LEDs.
    Aux_LED_Mutex = xSemaphoreCreateMutex ();
    // Same "exists before any lower-priority task can touch it" guarantee
    // Aux_LED_Mutex relies on above -- heartbeat_task must never be able to
    // call Clock_Set_Brightness before this exists.
    Brightness_Mutex = xSemaphoreCreateMutex ();
    for (uint Index = 0; Index < Aux_LED_Count; Index++)
    {
        Aux_LED_State [Index].Colour = Addressable_LED :: Black;
        Aux_LED_State [Index].Flash = false;
    } // for
} // Clock_Init


void Clock_Set (void)
{
    uint Hour_LED, Minute_LED, Second_LED;
    unsigned char Brightness;
    datetime_t Time;

    Flasher = !Flasher;
    xSemaphoreTake (RTC_Mutex, portMAX_DELAY);
    rtc_get_datetime(&Time);
    xSemaphoreGive (RTC_Mutex);
    Hour_LED = Time.hour % Clock_Hours;
    Minute_LED = Time.min / 5;
    Second_LED = Time.sec / 5;
    Clock_String->Solid (Addressable_LED :: Black);
    xSemaphoreTake (Brightness_Mutex, portMAX_DELAY);
    Brightness = Current_Brightness;
    xSemaphoreGive (Brightness_Mutex);
    if ((Hour_LED == Minute_LED) && (Hour_LED == Second_LED))
    { // hour, minute and second all coincide
        Clock_String->Set_One (Addressable_LED :: White, Hour_LED, Brightness);
    }
    else if (Hour_LED == Minute_LED)
    { // hour and minute coincide
        Clock_String->Set_One (Addressable_LED :: Yellow, Hour_LED, Brightness);
        Clock_String->Set_One (Addressable_LED :: Blue, Second_LED, Brightness);
    }
    else if (Hour_LED == Second_LED)
    { // hour and second coincide
        Clock_String->Set_One (Addressable_LED :: Magenta, Hour_LED, Brightness);
        Clock_String->Set_One (Addressable_LED :: Green, Minute_LED, Brightness);
    }
    else if (Second_LED == Minute_LED)
    { // minute and second coincide
        Clock_String->Set_One (Addressable_LED :: Cyan, Second_LED, Brightness);
        Clock_String->Set_One (Addressable_LED :: Red, Hour_LED, Brightness);
    }
    else
    { // no collision
        Clock_String->Set_One (Addressable_LED :: Red, Hour_LED, Brightness);
        Clock_String->Set_One (Addressable_LED :: Green, Minute_LED, Brightness);
        Clock_String->Set_One (Addressable_LED :: Blue, Second_LED, Brightness);
    } // LED collision handling
    // AM/PM indicator is LED 19
    if (Time.hour < 12)
    {
        Clock_String->Set_One (Addressable_LED :: Yellow, Clock_Count - 1, Brightness);
    }
    else
    {
        Clock_String->Set_One (Addressable_LED :: Cyan, Clock_Count - 1, Brightness);
    } // Time.hour < 12
    xSemaphoreTake (Aux_LED_Mutex, portMAX_DELAY);
    for (uint Index = 0; Index < Aux_LED_Count; Index++)
    {
        uint32_t Colour = Aux_LED_State [Index].Flash &&
          !Flasher ? Addressable_LED :: Black : Aux_LED_State [Index].Colour;
        Clock_String->Set_One (Colour, Aux_LED_First + Index, Brightness);
    } // for
    xSemaphoreGive (Aux_LED_Mutex);
    Clock_String->Update ();
} // Clock_Set


void Clock_Set_Aux_LEDs (const uint32_t Colour [Aux_LED_Count],
  const bool Flash [Aux_LED_Count])
{
    xSemaphoreTake (Aux_LED_Mutex, portMAX_DELAY);
    for (uint Index = 0; Index < Aux_LED_Count; Index++)
    {
        Aux_LED_State [Index].Colour = Colour [Index];
        Aux_LED_State [Index].Flash = Flash [Index];
    } // for
    xSemaphoreGive (Aux_LED_Mutex);
} // Clock_Set_Aux_LEDs

void Clock_Set_Brightness (unsigned char Brightness)
{
    xSemaphoreTake (Brightness_Mutex, portMAX_DELAY);
    Current_Brightness = Brightness;
    xSemaphoreGive (Brightness_Mutex);
} // Clock_Set_Brightness
