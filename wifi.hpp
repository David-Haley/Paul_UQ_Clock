// Wi-Fi connection management for Paul_UQ_Clock.
// Author : David Haley
// Created : 01/09/2026

#ifndef WIFI_HPP
#define WIFI_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Brings up the cyw43/lwIP async_context pinned to core 1, then connects to the
// network named by WiFi_SSID in configuration.h. Blocks until associated and
// DHCP-bound (or the connection attempt fails). Returns true on success.
bool WiFi_Connect (void);

#ifdef __cplusplus
}
#endif

#endif
