#pragma once

/**
 * @brief CLI FreeRTOS task
 * @param arg Task argument (unused)
 */
void cli_task(void *arg);

/**
 * @brief Register console commands
 */
void register_commands(void);

/**
 * @brief Channel minimum and maximum values
 */
extern int channel_min[CH_MAX];
extern int channel_max[CH_MAX];