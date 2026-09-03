// Wi-Fi connection management for Paul_UQ_Clock.
// Author : David Haley
// Created : 01/09/2026

#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "event_groups.h"

#include "pico/cyw43_arch.h"
#include "pico/async_context_freertos.h"

#include "wifi.hpp"
#include "configuration.h"

const uint32_t Connect_Timeout_Ms = 60000;

const EventBits_t Wifi_Connected_Bit = 1 << 0;
const EventBits_t Wifi_Failed_Bit = 1 << 1;

// Guards Started below, so that when ntp.cpp's task and aux_led.cpp's task
// both call WiFi_Connect at startup, exactly one of them does the actual
// connect and the other just waits for the result (see Wifi_Event_Group).
static SemaphoreHandle_t Start_Mutex = NULL;
static EventGroupHandle_t Wifi_Event_Group = NULL;
static bool Started = false;

// The cyw43 driver's own async_context worker task and lwIP's tcpip_thread (which
// it creates internally) are not pinned to a particular core: pinning the former
// to core 1 hung inside cyw43_arch_init on this SDK/FreeRTOS-Kernel combination,
// and the latter isn't reachable to pin at all without patching lwIP's sys_arch.
// Both default to configTASK_DEFAULT_CORE_AFFINITY (core 0), same as every other
// task in this project. See CLAUDE.md.
static async_context_freertos_t Async_Context;

void WiFi_Init (void)
{
    Start_Mutex = xSemaphoreCreateMutex ();
    Wifi_Event_Group = xEventGroupCreate ();
} // WiFi_Init

static bool Do_Connect (void)
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
} // Do_Connect

bool WiFi_Connect (void)
{
    xSemaphoreTake (Start_Mutex, portMAX_DELAY);
    if (!Started)
    {
        Started = true;
        xSemaphoreGive (Start_Mutex);
        bool Success = Do_Connect ();
        xEventGroupSetBits (Wifi_Event_Group,
          Success ? Wifi_Connected_Bit : Wifi_Failed_Bit);
        return Success;
    } // if
    xSemaphoreGive (Start_Mutex);
    EventBits_t Bits = xEventGroupWaitBits (Wifi_Event_Group,
      Wifi_Connected_Bit | Wifi_Failed_Bit, pdFALSE, pdFALSE, portMAX_DELAY);
    return (Bits & Wifi_Connected_Bit) != 0;
} // WiFi_Connect
