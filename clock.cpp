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

void Clock_Init (void)
{
    // Start on Tuesday of 01/09/2026 at 00:00:00
    datetime_t Time = {
        .year = 2026,
        .month = 9,
        .day = 1,
        .dotw = 2, // 0 is Sunday, so 2 is Tuesday
        .hour = 0,
        .min = 0,
        .sec = 0
    };
    rtc_init();
    rtc_set_datetime(&Time);
    sleep_us (64); // delay is required because of slow rtc hardware
    Clock_String =
      new Addressable_LED (Clock_Count, pio0, 0, Addressable_LED :: J1);
    Clock_String->Solid (Addressable_LED :: Black);
} // Clock_Init


void Clock_Set (void)
{
    uint Hour_LED, Minute_LED, Second_LED;
    datetime_t Time;

    rtc_get_datetime(&Time);
    Hour_LED = Time.hour % Clock_Hours;
    Minute_LED = Time.min / 5;
    Second_LED = Time.sec / 5;
    Clock_String->Solid (Addressable_LED :: Black);
    Clock_String->Set_One (Addressable_LED :: Red, Hour_LED);
    Clock_String->Set_One (Addressable_LED :: Green, Minute_LED);
    Clock_String->Set_One (Addressable_LED :: Blue, Second_LED);
    // AM/PM indicator is LED 19
    if (Time.hour < 12)
    {
        Clock_String->Set_One (Addressable_LED :: Yellow, Clock_Count - 1);
    }
    else
    {
        Clock_String->Set_One (Addressable_LED :: Cyan, Clock_Count - 1);
    } // Time.hour < 12
    Clock_String->Update ();
} // Clock_Set
