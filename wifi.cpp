// Wi-Fi connection management for Paul_UQ_Clock.
// Author : David Haley
// Created : 01/09/2026

#include <stdio.h>

#include "pico/cyw43_arch.h"
#include "pico/async_context_freertos.h"

#include "wifi.hpp"
#include "configuration.h"

const uint32_t Connect_Timeout_Ms = 60000;

// The cyw43 driver's own async_context worker task and lwIP's tcpip_thread (which
// it creates internally) are not pinned to a particular core: pinning the former
// to core 1 hung inside cyw43_arch_init on this SDK/FreeRTOS-Kernel combination,
// and the latter isn't reachable to pin at all without patching lwIP's sys_arch.
// Both default to configTASK_DEFAULT_CORE_AFFINITY (core 0), same as every other
// task in this project. See CLAUDE.md.
static async_context_freertos_t Async_Context;

bool WiFi_Connect (void)
{
    async_context_freertos_config_t Config =
      async_context_freertos_default_config ();
    if (!async_context_freertos_init (&Async_Context, &Config))
    {
        printf ("Wi-Fi: failed to create lwIP async context\n");
        return false;
    } // if
    cyw43_arch_set_async_context (&Async_Context.core);
    // Explicit country (rather than the default CYW43_COUNTRY_WORLDWIDE) speeds up
    // and stabilises channel negotiation.
    if (cyw43_arch_init_with_country (CYW43_COUNTRY_AUSTRALIA) != 0)
    {
        printf ("Wi-Fi: cyw43_arch_init failed\n");
        return false;
    } // if
    cyw43_arch_enable_sta_mode ();
    printf ("Wi-Fi: connecting...\n");
    if (cyw43_arch_wifi_connect_timeout_ms (WiFi_SSID, wiFi_Password,
        CYW43_AUTH_WPA2_AES_PSK, Connect_Timeout_Ms) != 0)
    {
        printf ("Wi-Fi: connection failed\n");
        return false;
    } // if
    printf ("Wi-Fi: connected\n");
    return true;
} // WiFi_Connect
