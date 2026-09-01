// NTP time synchronisation for Paul_UQ_Clock.
// Author : David Haley
// Created : 01/09/2026

#ifndef NTP_HPP
#define NTP_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Creates the FreeRTOS task that connects to Wi-Fi, sets the RTC from NTP
// (applying UTC_Offset from configuration.h) and resyncs once an hour.
void NTP_Start (void);

#ifdef __cplusplus
}
#endif

#endif
