#include "nvs.h"
#include "nvs_flash.h"
#include "adc.h"
#include "esp_log.h"

static const char *TAG = "NVS";
static nvs_handle_t nvs;

/**
 * @brief Initialize NVS and open handle
 */
void nvs_init(void) {
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_open("adc", NVS_READWRITE, &nvs));
}

void nvs_set_channel_i32(const char *prefix, int ch, int32_t val) {
    if(!check_channel(ch)) return;
    char key[16];
    sprintf(key, "%s%d", prefix, ch);
    nvs_set_i32(nvs, key, val);
    nvs_commit(nvs);
}

int32_t nvs_get_channel_i32(const char *prefix, int ch, int32_t def_val) {
    if(!check_channel(ch)) return def_val;
    char key[16];
    sprintf(key, "%s%d", prefix, ch);
    int32_t val = def_val;
    nvs_get_i32(nvs, key, &val);
    return val;
}
