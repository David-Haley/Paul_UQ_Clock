// GP7 CPU-load probe for Paul_UQ_Clock.
// Author : David Haley
// Created : 01/09/2026

#include <stdio.h>

#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"

#include "cpu_load.hpp"

const uint CPU_Load_Pin = 7;

// Debugging: tallies which tasks are switched in while non-idle, by name, so
// an unexpectedly busy CPU can be attributed to a specific task instead of
// guessed at. TODO remove once the observed GP7 duty cycle is explained.
const uint32_t Max_Tracked_Tasks = 8;
static const char *Task_Names [Max_Tracked_Tasks];
static volatile uint32_t Task_Counts [Max_Tracked_Tasks];
static volatile uint32_t Tracked_Task_Count = 0;
static volatile uint32_t Busy_Switch_Count = 0;

void CPU_Load_Init (void)
{
    gpio_init (CPU_Load_Pin);
    gpio_set_dir (CPU_Load_Pin, GPIO_OUT);
    gpio_put (CPU_Load_Pin, false);
} // CPU_Load_Init

void CPU_Load_Task_Switched_In (void)
{
    TaskHandle_t Current = xTaskGetCurrentTaskHandle ();
    bool Idle = (Current == xTaskGetIdleTaskHandle ());
    gpio_put (CPU_Load_Pin, Idle);
    if (Idle)
    {
        return;
    } // if
    Busy_Switch_Count++;
    const char *Name = pcTaskGetName (Current);
    for (uint32_t I = 0; I < Tracked_Task_Count; I++)
    {
        if (Task_Names [I] == Name)
        {
            Task_Counts [I]++;
            return;
        } // if
    } // for
    if (Tracked_Task_Count < Max_Tracked_Tasks)
    {
        Task_Names [Tracked_Task_Count] = Name;
        Task_Counts [Tracked_Task_Count] = 1;
        Tracked_Task_Count++;
    } // if
} // CPU_Load_Task_Switched_In

void CPU_Load_Report (void)
{
    printf ("CPU load: %lu busy switch-ins/s", (unsigned long) Busy_Switch_Count);
    Busy_Switch_Count = 0;
    for (uint32_t I = 0; I < Tracked_Task_Count; I++)
    {
        printf (", %s=%lu", Task_Names [I], (unsigned long) Task_Counts [I]);
        Task_Counts [I] = 0;
    } // for
    printf ("\n");
} // CPU_Load_Report
