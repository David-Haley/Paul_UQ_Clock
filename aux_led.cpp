// Auxiliary LED (indices 12-18) control via MQTT/JSON for Paul_UQ_Clock.
// Author : David Haley
// Created : 03/09/2026

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Must come before any lwIP header that pulls in lwip/sockets.h: with
// LWIP_COMPAT_SOCKETS on, sockets.h #defines "poll" to lwip_poll, which
// collides with an unrelated "poll" identifier used inside cyw43_arch.h's
// own headers if sockets.h is parsed first.
#include "pico/cyw43_arch.h"

#include "lwip/apps/mqtt.h"
#include "lwip/netdb.h"

#include "aux_led.hpp"
#include "wifi.hpp"
#include "clock.hpp"
#include "addressable_led.hpp"
#include "configuration.h"
#include "cJSON.h"

// Retry (DNS resolve failure, synchronous mqtt_client_connect failure, or a
// non-accepted Connection_Cb status) backs off from Mqtt_Retry_Initial_Ms up
// to Mqtt_Retry_Max_Ms, resetting to the initial delay once a connection is
// actually accepted — so a transient failure (e.g. the ERR_RTE routing race
// seen on hardware) recovers quickly, while a persistent one settles at the
// slower steady-state rate instead of hammering the broker.
const uint32_t Mqtt_Retry_Initial_Ms = 2000;
const uint32_t Mqtt_Retry_Max_Ms = 60000;
const uint32_t Wifi_Settle_Grace_Ms = 500;
const uint32_t Aux_LED_Task_Stack_Words = 1536; // headroom for cJSON parsing
const size_t Rx_Buffer_Size = 512; // comfortably fits 7 aux_led elements

const uint Aux_LED_Colours = 8;
static const char *Colour_Names [Aux_LED_Colours] =
  { "red", "yellow", "green", "cyan", "blue", "magenta", "white", "black" };
static const uint32_t Colour_Values [Aux_LED_Colours] =
  {
    Addressable_LED :: Red,
    Addressable_LED :: Yellow,
    Addressable_LED :: Green,
    Addressable_LED :: Cyan,
    Addressable_LED :: Blue,
    Addressable_LED :: Magenta,
    Addressable_LED :: White,
    Addressable_LED :: Black
  }; // Colour_Values

static mqtt_client_t *Client = NULL;
// Given by Connection_Cb whenever the connection is not (or is no longer)
// established, so the task's connect loop knows when to retry.
static SemaphoreHandle_t Disconnect_Semaphore = NULL;
// Only ever touched by Connection_Cb (reset on success) and by Aux_LED_Task
// while it isn't connected (grown after each failure) — the two never run
// concurrently, since the task is always blocked on Disconnect_Semaphore
// whenever a connection exists for Connection_Cb to later report lost.
static uint32_t Backoff_Ms = Mqtt_Retry_Initial_Ms;

static uint8_t Rx_Buffer [Rx_Buffer_Size];
static size_t Rx_Length = 0;
static bool Rx_Overflow = false;

static bool Lookup_Colour (const char *Name, uint32_t &Value)
{
    for (uint Index = 0; Index < Aux_LED_Colours; Index++)
    {
        if (strcmp (Name, Colour_Names [Index]) == 0)
        {
            Value = Colour_Values [Index];
            return true;
        } // if
    } // for
    return false;
} // Lookup_Colour

// Parses one complete MQTT payload and, if it is a valid aux_led JSON
// message (see "aux_led.json schema"), replaces the whole aux LED bank in one
// step: LEDs not mentioned in Text go black, per the schema being treated as
// authoritative for the whole 12-18 range on every message.
static void Apply_Message (const char *Text)
{
    cJSON *Root = cJSON_Parse (Text);
    if (Root == NULL)
    {
        printf ("Aux LED: invalid JSON\n");
        return;
    } // if
    cJSON *Array = cJSON_GetObjectItemCaseSensitive (Root, "aux_led");
    if (!cJSON_IsArray (Array))
    {
        printf ("Aux LED: JSON has no aux_led array\n");
        cJSON_Delete (Root);
        return;
    } // if

    uint32_t Colour [Aux_LED_Count];
    bool Flash [Aux_LED_Count];
    for (uint Index = 0; Index < Aux_LED_Count; Index++)
    {
        Colour [Index] = Addressable_LED :: Black;
        Flash [Index] = false;
    } // for

    cJSON *Element;
    cJSON_ArrayForEach (Element, Array)
    {
        cJSON *Led_Item = cJSON_GetObjectItemCaseSensitive (Element, "led");
        cJSON *Colour_Item =
          cJSON_GetObjectItemCaseSensitive (Element, "colour");
        cJSON *Flash_Item = cJSON_GetObjectItemCaseSensitive (Element, "flash");
        uint32_t Colour_Value;

        if (!cJSON_IsNumber (Led_Item) || !cJSON_IsString (Colour_Item))
        {
            printf ("Aux LED: skipping element with missing led/colour\n");
            continue;
        } // if
        int Led_Number = Led_Item->valueint;
        if (Led_Number < Aux_LED_First || Led_Number > Aux_LED_Last)
        {
            printf ("Aux LED: skipping out-of-range led %d\n", Led_Number);
            continue;
        } // if
        if (!Lookup_Colour (Colour_Item->valuestring, Colour_Value))
        {
            printf ("Aux LED: skipping unknown colour \"%s\"\n",
              Colour_Item->valuestring);
            continue;
        } // if
        Colour [Led_Number - Aux_LED_First] = Colour_Value;
        // Absent or non-boolean flash defaults to false (steady, not flashing).
        Flash [Led_Number - Aux_LED_First] = cJSON_IsTrue (Flash_Item);
    } // cJSON_ArrayForEach

    Clock_Set_Aux_LEDs (Colour, Flash);
    cJSON_Delete (Root);
} // Apply_Message

static void Publish_Cb (void *Arg, const char *Topic_Name, u32_t Tot_Len)
{
    Rx_Length = 0;
    Rx_Overflow = Tot_Len >= sizeof (Rx_Buffer);
    if (Rx_Overflow)
    {
        printf ("Aux LED: message too large (%lu bytes), ignoring\n",
          (unsigned long) Tot_Len);
    } // if
} // Publish_Cb

static void Data_Cb (void *Arg, const u8_t *Data, u16_t Len, u8_t Flags)
{
    if (!Rx_Overflow)
    {
        if (Rx_Length + Len < sizeof (Rx_Buffer))
        {
            memcpy (Rx_Buffer + Rx_Length, Data, Len);
            Rx_Length += Len;
        }
        else
        {
            Rx_Overflow = true;
            printf ("Aux LED: message too large, ignoring\n");
        } // if
    } // if
    if ((Flags & MQTT_DATA_FLAG_LAST) != 0)
    {
        if (!Rx_Overflow)
        {
            Rx_Buffer [Rx_Length] = 0;
            Apply_Message ((const char *) Rx_Buffer);
        } // if
    } // if
} // Data_Cb

static void Subscribe_Cb (void *Arg, err_t Err)
{
    if (Err == ERR_OK)
    {
        printf ("Aux LED: subscribed to %s\n", Topic);
    }
    else
    {
        printf ("Aux LED: subscribe failed, err %d\n", (int) Err);
    } // if
} // Subscribe_Cb

static void Connection_Cb (mqtt_client_t *Mqtt_Client, void *Arg,
  mqtt_connection_status_t Status)
{
    if (Status == MQTT_CONNECT_ACCEPTED)
    {
        printf ("Aux LED: MQTT connected\n");
        Backoff_Ms = Mqtt_Retry_Initial_Ms;
        cyw43_arch_lwip_begin ();
        mqtt_subscribe (Mqtt_Client, Topic, 0, Subscribe_Cb, NULL);
        cyw43_arch_lwip_end ();
    }
    else
    {
        printf ("Aux LED: MQTT connection status %d\n", (int) Status);
        xSemaphoreGive (Disconnect_Semaphore);
    } // if
} // Connection_Cb

// Waits the current backoff delay, then grows it for next time (capped).
static void Retry_Backoff (void)
{
    printf ("Aux LED: retrying MQTT connection in %lu ms\n",
      (unsigned long) Backoff_Ms);
    vTaskDelay (pdMS_TO_TICKS (Backoff_Ms));
    Backoff_Ms = Backoff_Ms * 2 < Mqtt_Retry_Max_Ms ?
      Backoff_Ms * 2 : Mqtt_Retry_Max_Ms;
} // Retry_Backoff

static bool Resolve_Broker (ip_addr_t &Address)
{
    struct addrinfo Hints = {};
    struct addrinfo *Result = NULL;

    Hints.ai_family = AF_INET;
    Hints.ai_socktype = SOCK_STREAM;
    if (lwip_getaddrinfo (Broker, NULL, &Hints, &Result) != 0 ||
        Result == NULL)
    {
        printf ("Aux LED: failed to resolve %s\n", Broker);
        return false;
    } // if
    Address.addr = ((struct sockaddr_in *) Result->ai_addr)->sin_addr.s_addr;
    lwip_freeaddrinfo (Result);
    return true;
} // Resolve_Broker

static void Aux_LED_Task (__unused void *Params)
{
    if (!WiFi_Connect ())
    {
        // No network: aux LEDs stay at whatever Clock_Init set them to.
        vTaskDelete (NULL);
    } // if
    // Grace period for the network stack's routing state to settle: a task
    // that wakes immediately when a second/later WiFi_Connect() caller
    // returns can otherwise call mqtt_client_connect before ip_route() can
    // find the just-brought-up netif, failing with ERR_RTE (confirmed on
    // hardware). Mirrors ntp.cpp's DHCP_NTP_Grace_Ms, for an unrelated
    // reason (that one waits for the DHCP-provided-NTP-server callback).
    vTaskDelay (pdMS_TO_TICKS (Wifi_Settle_Grace_Ms));

    Disconnect_Semaphore = xSemaphoreCreateBinary ();

    struct mqtt_connect_client_info_t Client_Info = {};
    Client_Info.client_id = "paul_uq_clock";
    Client_Info.client_user = User;
    Client_Info.client_pass = Password;
    Client_Info.keep_alive = 60;

    Client = mqtt_client_new ();
    cyw43_arch_lwip_begin ();
    mqtt_set_inpub_callback (Client, Publish_Cb, Data_Cb, NULL);
    cyw43_arch_lwip_end ();

    while (true)
    {
        ip_addr_t Broker_Addr;

        if (!Resolve_Broker (Broker_Addr))
        {
            Retry_Backoff ();
            continue;
        } // if
        cyw43_arch_lwip_begin ();
        err_t Err = mqtt_client_connect (Client, &Broker_Addr, MQTT_PORT,
          Connection_Cb, NULL, &Client_Info);
        cyw43_arch_lwip_end ();
        if (Err != ERR_OK)
        {
            printf ("Aux LED: mqtt_client_connect failed, err %d\n",
              (int) Err);
            Retry_Backoff ();
            continue;
        } // if
        // Blocks here for as long as the connection stays up; Connection_Cb
        // gives this semaphore on refusal, disconnection or timeout.
        xSemaphoreTake (Disconnect_Semaphore, portMAX_DELAY);
        Retry_Backoff ();
    } // while
} // Aux_LED_Task

void Aux_LED_Start (void)
{
    xTaskCreate (Aux_LED_Task, "aux_led", Aux_LED_Task_Stack_Words, NULL,
      tskIDLE_PRIORITY + 1, NULL);
} // Aux_LED_Start
