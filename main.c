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
#include "ntp.hpp"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"

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
    datetime_t Time;

    // RTC_Mutex is created by Clock_Init (called from led_task). Safe without
    // waiting for it here: led_task has higher priority and both tasks are
    // core-0-only, so led_task always runs Clock_Init to completion before
    // heartbeat_task gets any CPU time.
    while (true) {
        xSemaphoreTake(RTC_Mutex, portMAX_DELAY);
        rtc_get_datetime(&Time);
        xSemaphoreGive(RTC_Mutex);
        printf("%04d-%02d-%02d %02d:%02d:%02d\n", Time.year, Time.month,
          Time.day, Time.hour, Time.min, Time.sec);
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_DELAY_MS));
    }
}

int main(void) {
    stdio_init_all();

    xTaskCreate(led_task, "led", configMINIMAL_STACK_SIZE, NULL, LED_TASK_PRIORITY, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", configMINIMAL_STACK_SIZE, NULL, HEARTBEAT_TASK_PRIORITY, NULL);
    NTP_Start();

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
