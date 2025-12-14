/**
 * @file main.c
 * @brief Entry point for ADC + CLI system on ESP32.
 *
 * Initializes FreeRTOS tasks for ADC reading and command line interface.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "adc.h"
#include "cli.h"
#include "nvs.h"

/**
 * @brief Main application entry point.
 *
 * Initializes NVS, then starts the ADC and CLI FreeRTOS tasks.
 */
void app_main(void)
{
    nvs_init();
    xTaskCreate(adc_task, "adc_task", 4096, NULL, 5, NULL);
    xTaskCreate(cli_task, "cli_task", 8192, NULL, 4, NULL);
}

