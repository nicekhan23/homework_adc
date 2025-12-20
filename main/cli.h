#pragma once
#include "esp_err.h"
#include "adc.h"

void register_commands(void);

/**
 * @brief CLI FreeRTOS task
 */
void cli_init(void);

/**
 * @brief Channel configuration
 */
extern int channel_min[CH_MAX];
extern int channel_max[CH_MAX];