// VEML7700 ambient light sensor for Paul_UQ_Clock.
// Author : David Haley

#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "veml7700.hpp"

const uint VEML7700_SDA_Pin = 0;
const uint VEML7700_SCL_Pin = 1;
const uint32_t VEML7700_Baudrate_Hz = 100000; // 100 kHz standard mode; plenty
  // for a 1 Hz, 2-register read and the simplest/most robust choice.
const uint8_t VEML7700_Address = 0x10;
const uint8_t VEML7700_Reg_ALS_Conf = 0x00;
const uint8_t VEML7700_Reg_ALS = 0x04;
const uint VEML7700_Timeout_Us = 10000; // per I2C transaction; a NACK (no
  // device) returns almost immediately, this only bounds the pathological
  // "bus stuck" case, and even two timeouts/second is negligible against the
  // 1000 ms heartbeat period.
const uint32_t VEML7700_Power_On_Settle_Us = 2500; // datasheet settle time
  // after ALS_SD is cleared, before the first ALS reading is valid.
const uint16_t VEML7700_Raw_ALS_Max = 65535; // full 16-bit ALS register
  // range; Brightness scaling normalizes log2(Raw_ALS) against log2 of this.
const unsigned char VEML7700_Brightness_Min = 2; // matches
  // Addressable_LED::Set_One's dimmest non-zero Brightness.
const unsigned char VEML7700_Brightness_Max = 255; // matches
  // Addressable_LED::Set_One's unscaled Brightness.

static bool Device_Present = false;

// Writes ALS_CONF = 0x0000 (gain x1, integration time 100 ms, ALS_SD=0 i.e.
// powered on). Returns true if the sensor ACKed the full write.
static bool Power_On (void)
{
    uint8_t Buffer [3] = { VEML7700_Reg_ALS_Conf, 0x00, 0x00 };
    int Result = i2c_write_timeout_us (i2c0, VEML7700_Address, Buffer, 3,
      false, VEML7700_Timeout_Us);
    if (Result != 3)
    {
        return false;
    } // if
    sleep_us (VEML7700_Power_On_Settle_Us);
    return true;
} // Power_On

void VEML7700_Init (void)
{
    i2c_init (i2c0, VEML7700_Baudrate_Hz);
    gpio_set_function (VEML7700_SDA_Pin, GPIO_FUNC_I2C);
    gpio_set_function (VEML7700_SCL_Pin, GPIO_FUNC_I2C);
    gpio_pull_up (VEML7700_SDA_Pin);
    gpio_pull_up (VEML7700_SCL_Pin);
    Device_Present = Power_On ();
    if (!Device_Present)
    {
        printf ("VEML7700: not detected at init, will keep retrying\n");
    } // if
} // VEML7700_Init

bool VEML7700_Read_ALS (uint16_t *Raw_ALS)
{
    if (!Device_Present)
    {
        // Cheap to retry every call: a NACK returns almost instantly, so
        // this gives self-healing (sensor plugged in later) with no
        // meaningful cost when it's genuinely absent.
        Device_Present = Power_On ();
        if (!Device_Present)
        {
            return false;
        } // if
    } // if

    uint8_t Command = VEML7700_Reg_ALS;
    uint8_t Data [2];
    int Result = i2c_write_timeout_us (i2c0, VEML7700_Address, &Command, 1,
      true, VEML7700_Timeout_Us); // nostop=true: repeated start into the read
    if (Result != 1)
    {
        Device_Present = false;
        return false;
    } // if
    Result = i2c_read_timeout_us (i2c0, VEML7700_Address, Data, 2, false,
      VEML7700_Timeout_Us);
    if (Result != 2)
    {
        Device_Present = false;
        return false;
    } // if
    *Raw_ALS = (uint16_t) Data [0] | ((uint16_t) Data [1] << 8); // LSB first
    return true;
} // VEML7700_Read_ALS

unsigned char VEML7700_Raw_To_Brightness (uint16_t Raw_ALS)
{
    float Clamped_Raw = Raw_ALS < 1 ? 1.0f : (float) Raw_ALS;
    float Ratio = log2f (Clamped_Raw) / log2f ((float) VEML7700_Raw_ALS_Max);
    float Brightness = (float) VEML7700_Brightness_Min + Ratio *
      (float) (VEML7700_Brightness_Max - VEML7700_Brightness_Min);
    if (Brightness < (float) VEML7700_Brightness_Min)
    {
        Brightness = (float) VEML7700_Brightness_Min;
    } // if
    else if (Brightness > (float) VEML7700_Brightness_Max)
    {
        Brightness = (float) VEML7700_Brightness_Max;
    } // if
    return (unsigned char) (Brightness + 0.5f); // round to nearest
} // VEML7700_Raw_To_Brightness
