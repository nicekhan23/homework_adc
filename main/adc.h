#pragma once
#include <stdbool.h>

/**
 * @brief Number of ADC channels
 */
#define CH_MAX 6

/**
 * @brief Smoothing factor for running average
 */
#define AVG_SMOOTH 10

/* Global ADC arrays */
extern int adc_raw[CH_MAX];
extern int adc_avg[CH_MAX];
extern int adc_filtered[CH_MAX];
extern int hysteresis[CH_MAX];

/**
 * @brief Check if the channel index is valid
 * @param ch Channel index
 * @return true if valid, false otherwise
 */
bool check_channel(int ch);

/**
 * @brief ADC task for FreeRTOS
 * @param arg Task argument (unused)
 */
void adc_task(void *arg);

/**
 * @brief Get filtered ADC value for a channel
 * @param ch Channel index
 * @return Filtered ADC value or -1 if invalid
 */
int adc_get(int ch);

/**
 * @brief Get normalized ADC value (0..1)
 * @param ch Channel index
 * @return Normalized value or -1.0 if invalid
 */
float adc_get_normalized(int ch);
