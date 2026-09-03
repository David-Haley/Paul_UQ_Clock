// Auxiliary LED (indices 12-18) control via MQTT/JSON for Paul_UQ_Clock.
// Author : David Haley
// Created : 03/09/2026

#ifndef AUX_LED_HPP
#define AUX_LED_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Creates the FreeRTOS task that connects to Wi-Fi (sharing the connection
// with ntp.cpp via WiFi_Connect), subscribes to the MQTT topic named by
// Broker/User/Topic/Password in configuration.h, and applies each parsed
// aux_led JSON message (see "aux_led.json schema") via Clock_Set_Aux_LEDs.
void Aux_LED_Start (void);

#ifdef __cplusplus
}
#endif

#endif
