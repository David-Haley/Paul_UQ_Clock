// Software to test individula LEDs on Paul's clock display board 20 LEDs.
// Note this has been built for the PI Pico W
// Author    : David Haley
// Created   : 26/11/2022
// Last Edit : 31/09/2025
// 20231105: Setting to black moved to start of cycle so that the LED data is
// not overwritten before the DMA process has completed. 

#include "pico/stdlib.h"
#include "clock.hpp"
#include "addressable_led.hpp"

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

void Clock_Init (void)
{
    Clock_String = new Addressable_LED (Clock_Count, pio0, 0, Addressable_LED :: J1);
    Clock_String->Solid (Addressable_LED :: Black);
    Clock_String->Update ();
} // Clock_Init


void Clock_Set (void)
{
    static uint L = 0;
    Clock_String->Solid (Addressable_LED :: Black);
    Clock_String->Set_One (Addressable_LED :: White, L);
    Clock_String->Update ();
    if (L < Clock_Count)
    {
        L++;
    }
    else
    {
        L = 0;
    } // (L < Clock_Count)
} // Clock_Set
