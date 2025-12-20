#include "adc.h"
#include "driver/adc.h"
#include "nvs.h"
#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"


static const char *TAG = "ADC";

/* ADC channel mapping */
static adc1_channel_t adc_channels[CH_MAX] = {
    ADC1_CHANNEL_0,
    ADC1_CHANNEL_3,
    ADC1_CHANNEL_6,
    ADC1_CHANNEL_7,
    ADC1_CHANNEL_4,
    ADC1_CHANNEL_5
};

int adc_raw[CH_MAX] = {0};
int adc_avg[CH_MAX] = {0};
int adc_filtered[CH_MAX] = {0};
int hysteresis[CH_MAX] = {10,10,10,10,10,10};

bool check_channel(int ch) { return (ch >= 0 && ch < CH_MAX); }

/**
 * @brief ADC FreeRTOS task.
 *
 * Reads raw ADC values, calculates running average, applies hysteresis,
 * and saves new filtered values to NVS if changed.
 *
 * @param arg Task argument (unused)
 */
void adc_task(void *arg)
{
    static int last_saved[CH_MAX] = {0};

    /* Configure ADC attenuation once */
    for(int ch=0; ch<CH_MAX; ch++) {
        adc1_config_channel_atten(adc_channels[ch], ADC_ATTEN_DB_12);
    }

    adc1_config_width(ADC_WIDTH_BIT_12);

    ESP_LOGI(TAG, "ADC task started, monitoring %d channels", CH_MAX);

    while(1) {
        for(int ch=0; ch<CH_MAX; ch++) {
            adc_raw[ch] = adc1_get_raw(adc_channels[ch]);
            adc_avg[ch] = adc_avg[ch] - adc_avg[ch]/AVG_SMOOTH + adc_raw[ch]/AVG_SMOOTH;

            if(adc_avg[ch] > adc_filtered[ch] + hysteresis[ch])
                adc_filtered[ch] = adc_avg[ch];
            else if(adc_avg[ch] < adc_filtered[ch] - hysteresis[ch])
                adc_filtered[ch] = adc_avg[ch];

            // Apply min/max scaling from NVS configuration
            int32_t min_val = nvs_get_channel_i32("ch_min", ch, 0);
            int32_t max_val = nvs_get_channel_i32("ch_max", ch, 4095);

            if (max_val > min_val) {
                adc_filtered[ch] = min_val + ((adc_filtered[ch] * (max_val - min_val)) / 4095);
            } else {
                adc_filtered[ch] = min_val;
            }

            if(adc_filtered[ch] != last_saved[ch]) {
                last_saved[ch] = adc_filtered[ch];
                nvs_set_channel_i32("ch_val", ch, adc_filtered[ch]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

int adc_get(int ch) {
    if(!check_channel(ch)) return -1;
    return adc_filtered[ch];
}

float adc_get_normalized(int ch) {
    if(!check_channel(ch)) return -1.0f;
    return (float)adc_filtered[ch]/4095.0f;
}