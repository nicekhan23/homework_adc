#pragma once
#include <stdint.h>

/**
 * @brief Initialize NVS
 */
void nvs_init(void);

/**
 * @brief Save int32 value for a specific channel
 */
void nvs_set_channel_i32(const char *prefix, int ch, int32_t val);

/**
 * @brief Read int32 value for a specific channel
 * @return Stored value or def_val if not found
 */
int32_t nvs_get_channel_i32(const char *prefix, int ch, int32_t def_val);