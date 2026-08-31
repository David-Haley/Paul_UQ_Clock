// Copied from free RTOS blink example, to become main programo of Psul's UQ
// clock.

// Modified by : David Haley
// Copied      : 31/08/2026
// Last Edited : 31/08/2026

#include <stdio.h>

#include "pico/stdlib.h"
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "clock.hpp"

#define LED_TASK_PRIORITY       (tskIDLE_PRIORITY + 2)
#define HEARTBEAT_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#define LED_DELAY_MS             1000
#define HEARTBEAT_DELAY_MS       1000

static void led_task(__unused void *params) {
    Clock_Init ();

    while (true) {
        Clock_Set ();
        vTaskDelay(pdMS_TO_TICKS(LED_DELAY_MS));
    }
}

static void heartbeat_task(__unused void *params) {
    uint32_t beat = 0;
    while (true) {
        printf("heartbeat %lu\n", (unsigned long) beat++);
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_DELAY_MS));
    }
}

int main(void) {
    stdio_init_all();

    xTaskCreate(led_task, "led", configMINIMAL_STACK_SIZE, NULL, LED_TASK_PRIORITY, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", configMINIMAL_STACK_SIZE, NULL, HEARTBEAT_TASK_PRIORITY, NULL);

    vTaskStartScheduler();

    /* Should never reach here. */
    while (true) {
    }
}

void vApplicationMallocFailedHook(void) {
    panic("FreeRTOS malloc failed");
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    panic("FreeRTOS stack overflow in task %s", pcTaskName);
}
