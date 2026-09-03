// Wi-Fi connection management for Paul_UQ_Clock.
// Author : David Haley
// Created : 01/09/2026

#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "event_groups.h"

#include "pico/cyw43_arch.h"
#include "pico/async_context_freertos.h"
#include "pico/time.h"

#include "task.h"

#include "wifi.hpp"
#include "configuration.h"

const uint32_t Connect_Timeout_Ms = 60000; // per-attempt join timeout
// Keep retrying (with backoff) for at least this long before giving up, since
// a single join attempt can fail outright on transient AP/RF issues (seen on
// hardware) despite the network being fine moments later.
const uint32_t Connect_Retry_Budget_Ms = 120000;
const uint32_t Connect_Backoff_Initial_Ms = 2000;
const uint32_t Connect_Backoff_Max_Ms = 15000;

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

static bool Init_Driver (void)
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
    return true;
} // Init_Driver

static bool Do_Connect (void)
{
    if (!Init_Driver ())
    {
        return false;
    } // if

    // A single join attempt can fail outright (transient AP/RF issue, seen on
    // hardware) even though the network is fine moments later, so retry with
    // backoff for at least Connect_Retry_Budget_Ms before giving up. The
    // driver/async context above are only ever set up once, per this
    // function's caller (WiFi_Connect's Started guard) — only the join itself
    // is repeated.
    absolute_time_t Deadline = make_timeout_time_ms (Connect_Retry_Budget_Ms);
    uint32_t Backoff_Ms = Connect_Backoff_Initial_Ms;
    uint Attempt = 0;

    while (true)
    {
        Attempt++;
        printf ("Wi-Fi: connecting (attempt %u)...\n", Attempt);
        if (cyw43_arch_wifi_connect_timeout_ms (WiFi_SSID, wiFi_Password,
            CYW43_AUTH_WPA2_AES_PSK, Connect_Timeout_Ms) == 0)
        {
            printf ("Wi-Fi: connected\n");
            return true;
        } // if
        if (absolute_time_diff_us (get_absolute_time (), Deadline) <= 0)
        {
            printf ("Wi-Fi: connection failed after %u attempt(s), giving up\n",
              Attempt);
            return false;
        } // if
        printf ("Wi-Fi: connection failed, retrying in %lu ms\n",
          (unsigned long) Backoff_Ms);
        vTaskDelay (pdMS_TO_TICKS (Backoff_Ms));
        Backoff_Ms = Backoff_Ms * 2 < Connect_Backoff_Max_Ms ?
          Backoff_Ms * 2 : Connect_Backoff_Max_Ms;
    } // while
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
