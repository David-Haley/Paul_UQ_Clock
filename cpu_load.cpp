// GP7 CPU-load probe for Paul_UQ_Clock.
// Author : David Haley
// Created : 01/09/2026

#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"

#include "cpu_load.hpp"

const uint CPU_Load_Pin = 7;

void CPU_Load_Init (void)
{
    gpio_init (CPU_Load_Pin);
    gpio_set_dir (CPU_Load_Pin, GPIO_OUT);
    gpio_put (CPU_Load_Pin, false);
} // CPU_Load_Init

void CPU_Load_Task_Switched_In (void)
{
    gpio_put (CPU_Load_Pin,
      xTaskGetCurrentTaskHandle () == xTaskGetIdleTaskHandle ());
} // CPU_Load_Task_Switched_In
