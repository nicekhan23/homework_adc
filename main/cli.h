#pragma once
#include "adc.h"

/**
 * @brief Initialize CLI subsystem
 * @return ESP_OK on success
 */
esp_err_t cli_init(void);

/**
 * @brief CLI FreeRTOS task
 */
void cli_task(void *arg);

/**
 * @brief Channel configuration
 */
extern int channel_min[CH_MAX];
extern int channel_max[CH_MAX];