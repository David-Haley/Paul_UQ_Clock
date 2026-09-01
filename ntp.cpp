// NTP time synchronisation for Paul_UQ_Clock.
// Author : David Haley
// Created : 01/09/2026

#include <stdio.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dhcp.h"

#include "hardware/rtc.h"
#include "pico/time.h"

#include "ntp.hpp"
#include "wifi.hpp"
#include "clock.hpp"
#include "configuration.h"

const char *Pool_NTP_Server = "pool.ntp.org";
const uint16_t NTP_Port = 123;
const size_t NTP_Packet_Size = 48;
const uint32_t NTP_Unix_Epoch_Diff = 2208988800UL; // seconds from 1900 to 1970
const uint32_t NTP_Recv_Timeout_Ms = 5000;
const uint32_t DHCP_NTP_Grace_Ms = 500;
const uint32_t NTP_Resync_Period_Ms = 3600000; // one hour, per Instructions.txt
const uint32_t NTP_Retry_Period_Ms = 60000; // used only if a sync attempt fails
const uint32_t NTP_Task_Stack_Words = 1024;

static SemaphoreHandle_t DHCP_NTP_Mutex = NULL;
static ip4_addr_t DHCP_NTP_Server;
static bool DHCP_NTP_Available = false;

// Called by lwIP's DHCP client (from lwIP's own tcpip_thread) if the DHCP server
// offers NTP server addresses (option 42). Remembers the first one, for
// Get_NTP_Server to prefer over the pool.ntp.org fallback (Instructions.txt #7).
extern "C" void dhcp_set_ntp_servers (u8_t Num_Ntp_Servers,
  const ip4_addr_t *Ntp_Server_Addrs)
{
    if (Num_Ntp_Servers > 0)
    {
        xSemaphoreTake (DHCP_NTP_Mutex, portMAX_DELAY);
        DHCP_NTP_Server = Ntp_Server_Addrs [0];
        DHCP_NTP_Available = true;
        xSemaphoreGive (DHCP_NTP_Mutex);
    } // if
} // dhcp_set_ntp_servers

static bool Get_NTP_Server (ip4_addr_t &Server_Addr)
{
    bool Have_Server;

    xSemaphoreTake (DHCP_NTP_Mutex, portMAX_DELAY);
    Have_Server = DHCP_NTP_Available;
    if (Have_Server)
    {
        Server_Addr = DHCP_NTP_Server;
    } // if
    xSemaphoreGive (DHCP_NTP_Mutex);
    if (Have_Server)
    {
        printf ("NTP: using DHCP-provided server\n");
        return true;
    } // if

    struct addrinfo Hints = {};
    struct addrinfo *Result = NULL;
    Hints.ai_family = AF_INET;
    Hints.ai_socktype = SOCK_DGRAM;
    if (lwip_getaddrinfo (Pool_NTP_Server, NULL, &Hints, &Result) != 0 ||
        Result == NULL)
    {
        printf ("NTP: failed to resolve %s\n", Pool_NTP_Server);
        return false;
    } // if
    Server_Addr.addr =
      ((struct sockaddr_in *) Result->ai_addr)->sin_addr.s_addr;
    lwip_freeaddrinfo (Result);
    printf ("NTP: using %s\n", Pool_NTP_Server);
    return true;
} // Get_NTP_Server

static bool Query_NTP (const ip4_addr_t &Server_Addr, time_t &Utc_Unix_Time)
{
    bool Result = false;
    int Sock = lwip_socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (Sock < 0)
    {
        printf ("NTP: socket() failed\n");
        return false;
    } // if
    struct timeval Timeout = { NTP_Recv_Timeout_Ms / 1000, 0 };
    lwip_setsockopt (Sock, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof (Timeout));

    struct sockaddr_in Server = {};
    Server.sin_family = AF_INET;
    Server.sin_port = lwip_htons (NTP_Port);
    Server.sin_addr.s_addr = Server_Addr.addr;

    uint8_t Packet [NTP_Packet_Size] = {0};
    Packet [0] = 0x23; // LI = 0, VN = 4, Mode = 3 (client)

    if (lwip_sendto (Sock, Packet, sizeof (Packet), 0,
        (struct sockaddr *) &Server, sizeof (Server)) == (int) sizeof (Packet))
    {
        struct sockaddr_in From;
        socklen_t From_Len = sizeof (From);
        int Received = lwip_recvfrom (Sock, Packet, sizeof (Packet), 0,
          (struct sockaddr *) &From, &From_Len);
        if (Received == (int) sizeof (Packet))
        {
            // Transmit Timestamp seconds field, RFC 4330, offset 40, big-endian.
            uint32_t Seconds_Since_1900 =
              ((uint32_t) Packet [40] << 24) | ((uint32_t) Packet [41] << 16) |
              ((uint32_t) Packet [42] << 8) | (uint32_t) Packet [43];
            Utc_Unix_Time = (time_t) (Seconds_Since_1900 - NTP_Unix_Epoch_Diff);
            Result = true;
        }
        else
        {
            printf ("NTP: no response from server\n");
        } // if
    }
    else
    {
        printf ("NTP: send failed\n");
    } // if
    lwip_close (Sock);
    return Result;
} // Query_NTP

static void Set_RTC_From_Unix (time_t Utc_Unix_Time)
{
    time_t Local_Unix_Time = Utc_Unix_Time + (time_t) UTC_Offset * 60;
    struct tm Local_Tm;

    gmtime_r (&Local_Unix_Time, &Local_Tm);
    datetime_t Time = {
        .year = (int16_t) (Local_Tm.tm_year + 1900),
        .month = (int8_t) (Local_Tm.tm_mon + 1),
        .day = (int8_t) Local_Tm.tm_mday,
        .dotw = (int8_t) Local_Tm.tm_wday,
        .hour = (int8_t) Local_Tm.tm_hour,
        .min = (int8_t) Local_Tm.tm_min,
        .sec = (int8_t) Local_Tm.tm_sec
    };
    xSemaphoreTake (RTC_Mutex, portMAX_DELAY);
    rtc_set_datetime (&Time);
    sleep_us (64); // delay is required because of slow rtc hardware
    xSemaphoreGive (RTC_Mutex);
    printf ("NTP: RTC set to %04d-%02d-%02d %02d:%02d:%02d local\n",
      Time.year, Time.month, Time.day, Time.hour, Time.min, Time.sec);
} // Set_RTC_From_Unix

static void NTP_Task (__unused void *Params)
{
    DHCP_NTP_Mutex = xSemaphoreCreateMutex ();
    if (!WiFi_Connect ())
    {
        // No network: the RTC keeps the Clock_Init placeholder time.
        vTaskDelete (NULL);
    } // if
    // Grace period for the DHCP-offered NTP server (option 42) callback to run.
    vTaskDelay (pdMS_TO_TICKS (DHCP_NTP_Grace_Ms));

    TickType_t Last_Wake_Time = xTaskGetTickCount ();
    while (true)
    {
        ip4_addr_t Server_Addr;
        time_t Utc_Unix_Time;
        bool Synced = Get_NTP_Server (Server_Addr) &&
          Query_NTP (Server_Addr, Utc_Unix_Time);

        if (Synced)
        {
            Set_RTC_From_Unix (Utc_Unix_Time);
        } // if
        vTaskDelayUntil (&Last_Wake_Time, pdMS_TO_TICKS (
          Synced ? NTP_Resync_Period_Ms : NTP_Retry_Period_Ms));
    } // while
} // NTP_Task

void NTP_Start (void)
{
    xTaskCreate (NTP_Task, "ntp", NTP_Task_Stack_Words, NULL,
      tskIDLE_PRIORITY + 1, NULL);
} // NTP_Start
