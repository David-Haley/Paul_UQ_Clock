// GP7 CPU-load probe for Paul_UQ_Clock: driven high while the idle task is
// running, low whenever any other task is running, so it can be watched on a
// scope/logic analyser to see how busy the CPU is.
// Author : David Haley
// Created : 01/09/2026

#ifndef CPU_LOAD_HPP
#define CPU_LOAD_HPP

#ifdef __cplusplus
extern "C" {
#endif

// Configures GP7 as an output. Call once, before vTaskStartScheduler().
void CPU_Load_Init (void);

// Called by FreeRTOS on every task switch (see the traceTASK_SWITCHED_IN
// definition in FreeRTOSConfig.h).
void CPU_Load_Task_Switched_In (void);

#ifdef __cplusplus
}
#endif

#endif
