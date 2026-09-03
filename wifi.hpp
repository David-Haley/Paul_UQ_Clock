// Wi-Fi connection management for Paul_UQ_Clock.
// Author : David Haley
// Created : 01/09/2026

#ifndef WIFI_HPP
#define WIFI_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Creates the primitives WiFi_Connect uses to make itself safe to call from
// more than one task (ntp.cpp and aux_led.cpp both need a connected Wi-Fi
// link). Must be called once, from main(), before any task that might call
// WiFi_Connect is created.
void WiFi_Init (void);

// Brings up the cyw43/lwIP async_context, then connects to the network named
// by WiFi_SSID in configuration.h. Blocks until associated and DHCP-bound (or
// the connection attempt fails). Safe to call from multiple tasks: only the
// first caller does the actual work, everyone else blocks until it finishes
// and gets the same result. Returns true on success.
bool WiFi_Connect (void);

#ifdef __cplusplus
}
#endif

#endif
