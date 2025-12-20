#include "cli.h"
#include "adc.h"
#include "nvs.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"

#define PROMPT "> "

static struct {
    struct arg_lit *help;
    struct arg_int *channel;
    struct arg_int *min;
    struct arg_int *max;
    struct arg_int *hyst;
    struct arg_lit *start;
    struct arg_end *end;
} args;

/**
 * @brief Scale raw ADC value to configured range with hysteresis
 * @param raw_adc Raw ADC value (0-4095)
 * @param min_val Configured minimum value
 * @param max_val Configured maximum value
 * @param hyst_val Hysteresis value
 * @param prev_scaled Previous scaled value (for hysteresis)
 * @return Scaled value with hysteresis applied
 */
static int scale_adc_value(int raw_adc, int32_t min_val, int32_t max_val, 
                          int32_t hyst_val, int prev_scaled) {
    if (max_val <= min_val) {
        return min_val;
    }
    
    // Map raw ADC (0-4095) to configured range (min-max)
    int new_scaled = min_val + ((raw_adc * (max_val - min_val)) / 4095);
    
    // Apply hysteresis
    if (abs(new_scaled - prev_scaled) < hyst_val) {
        return prev_scaled;  // Stay at previous value if within hysteresis
    }
    
    return new_scaled;
}

/**
 * @brief Print channel configuration and current values
 */
static void print_channel_info(void) {
    printf("\n=== ADC Channel Configuration ===\n");
    for (int i = 0; i < CH_MAX; i++) {
        int32_t min_val = nvs_get_channel_i32("ch_min", i, 0);
        int32_t max_val = nvs_get_channel_i32("ch_max", i, 4095);
        int32_t hyst_val = nvs_get_channel_i32("ch_hyst", i, 10);
        int raw_adc = adc_filtered[i];
        
        // Map raw ADC (0-4095) to configured range (min-max)
        int scaled_value;
        if (max_val > min_val) {
            scaled_value = min_val + ((raw_adc * (max_val - min_val)) / 4095);
        } else {
            scaled_value = min_val;
        }
        
        printf("CH%d: min=%4ld, max=%4ld, hyst=%3ld, raw=%4d, scaled=%4d\n",
               i, min_val, max_val, hyst_val, raw_adc, scaled_value);
    }
    printf("=================================\n");
}

/**
 * @brief Validate channel configuration
 * @return true if valid, false otherwise
 */
static bool validate_config(int ch, int min, int max, int hyst) {
    if (ch < 0 || ch >= CH_MAX) {
        printf("Error: Channel must be 0-%d\n", CH_MAX-1);
        return false;
    }
    
    if (min < 0 || min > 4095) {
        printf("Error: Min must be 0-4095\n");
        return false;
    }
    
    if (max < 0 || max > 4095) {
        printf("Error: Max must be 0-4095\n");
        return false;
    }
    
    if (hyst < 0 || hyst > 500) {
        printf("Error: Hysteresis must be 0-500\n");
        return false;
    }
    
    if (min > max) {
        printf("Error: Min (%d) cannot be greater than Max (%d)\n", min, max);
        return false;
    }
    
    return true;
}

/**
 * @brief Configuration command handler
 */
static int cmd_config(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void *)&args);
    
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        return 1;
    }

    // Show start information
    if (args.start->count > 0) {
        print_channel_info();
        return 0;
    }

    // Check if channel is required for other operations
    if ((args.min->count > 0 || args.max->count > 0 || args.hyst->count > 0) && 
        args.channel->count == 0) {
        printf("Error: Channel (-c) is required when setting min/max/hyst\n");
        return 1;
    }

    // Handle channel-specific operations
    if (args.channel->count > 0) {
        int ch = args.channel->ival[0];
        
        if (ch < 0 || ch >= CH_MAX) {
            printf("Error: Invalid channel. Must be 0-%d\n", CH_MAX-1);
            return 1;
        }

        // Get current values from NVS
        int current_min = nvs_get_channel_i32("ch_min", ch, 0);
        int current_max = nvs_get_channel_i32("ch_max", ch, 4095);
        int current_hyst = nvs_get_channel_i32("ch_hyst", ch, 10);
        
        // Apply new values if provided
        int new_min = (args.min->count > 0) ? args.min->ival[0] : current_min;
        int new_max = (args.max->count > 0) ? args.max->ival[0] : current_max;
        int new_hyst = (args.hyst->count > 0) ? args.hyst->ival[0] : current_hyst;

        // Validate configuration
        if (!validate_config(ch, new_min, new_max, new_hyst)) {
            return 1;
        }

        // Save new values to NVS
        bool changed = false;
        
        if (new_min != current_min) {
            nvs_set_channel_i32("ch_min", ch, new_min);
            printf("CH%d min set to %d\n", ch, new_min);
            changed = true;
        }
        
        if (new_max != current_max) {
            nvs_set_channel_i32("ch_max", ch, new_max);
            printf("CH%d max set to %d\n", ch, new_max);
            changed = true;
        }
        
        if (new_hyst != current_hyst) {
            nvs_set_channel_i32("ch_hyst", ch, new_hyst);
            printf("CH%d hysteresis set to %d\n", ch, new_hyst);
            changed = true;
        }
        
        if (changed) {
            printf("Changes saved to NVS for CH%d\n", ch);
        } else {
            printf("No changes made for CH%d\n", ch);
        }
    }

    return 0;
}

/**
 * @brief Register configuration command
 */
static void register_config_command(void) {
    args.help = arg_litn("h", "help", 0, 1, "Show help");
    args.channel = arg_intn("c", "channel", "<n>", 0, 1, "Channel number (0-5)");
    args.min = arg_intn("m", "min", "<val>", 0, 1, "Minimum value (0-4095)");
    args.max = arg_intn("M", "max", "<val>", 0, 1, "Maximum value (0-4095)");
    args.hyst = arg_intn("H", "hyst", "<val>", 0, 1, "Hysteresis (0-500)");
    args.start = arg_litn("s", "start", 0, 1, "Show channel information");
    args.end = arg_end(10);

    esp_console_cmd_t cmd = {
        .command = "config",
        .help = "Configure ADC channels",
        .hint = NULL,
        .func = &cmd_config,
        .argtable = &args
    };

    esp_console_cmd_register(&cmd);
}

/**
 * @brief Initialize and start CLI
 */
void cli_init(void)
{
    ESP_LOGI("CLI", "Initializing CLI");
    
    // Initialize console
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "CMD> ";
    repl_config.max_cmdline_length = 256;

    // Initialize UART for console
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    // Register commands
    register_config_command();
    
    ESP_LOGI("CLI", "Starting REPL");
    
    // Start the REPL (this blocks)
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}