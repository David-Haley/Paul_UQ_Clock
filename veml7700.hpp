// VEML7700 ambient light sensor (I2C0, SDA=GP0, SCL=GP1) for Paul_UQ_Clock.
// Only heartbeat_task touches this bus, so no mutex is needed here (contrast
// RTC_Mutex/Aux_LED_Mutex in clock.hpp, which guard state shared across
// tasks). If a second task ever needs I2C0, add one then.
// Author : David Haley

#ifndef VEML7700_HPP
#define VEML7700_HPP

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configures I2C0 (GP0=SDA, GP1=SCL) and attempts to power on the VEML7700.
// Call once, from the task that will call VEML7700_Read_ALS, before its own
// loop starts. i2c_init/gpio_set_function have no error return on this SDK,
// so the only detectable failure -- the sensor not ACKing -- is reported
// lazily by VEML7700_Read_ALS, which retries detection on every call.
void VEML7700_Init (void);

// Reads the raw 16-bit ALS register into *Raw_ALS and returns true on
// success. Returns false, leaving *Raw_ALS unchanged, if the sensor has
// never been detected or any I2C transfer fails/NACKs; also retries the
// power-on write in that case, so the sensor self-heals if plugged in later.
bool VEML7700_Read_ALS (uint16_t *Raw_ALS);

#ifdef __cplusplus
}
#endif

#endif
