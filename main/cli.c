#include "cli.h"
#include "adc.h"
#include "nvs.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "esp_log.h"
#include "linenoise/linenoise.h"

#define CMD_CONFIG "config"

int channel_min[CH_MAX] = {0, 0, 0, 0, 0, 0};
int channel_max[CH_MAX] = {4095, 4095, 4095, 4095, 4095, 4095};

static struct {
    struct arg_lit *help;
    struct arg_lit *read;
    struct arg_lit *write;
    struct arg_int *channel;
    struct arg_int *minval;
    struct arg_int *maxval;
    struct arg_lit *status;
    struct arg_int *hyst;
    struct arg_end *end;
} config_args;

/**
 * @brief Configure ADC channel settings via CLI
 */
static void print_help_config(void) {
    arg_print_syntax(stdout, (void *)&config_args, "\n");
    arg_print_glossary(stdout, (void *)&config_args, "  %-25s %s\n");
}

static void print_channel_status(void) {
    printf("\n=== Channel Status ===\n");
    for (int i = 0; i < CH_MAX; i++) {
        printf("CH%d: min=%4d, max=%4d, raw=%4d, avg=%4d, filtered=%4d, hyst=%d\n", 
                i, channel_min[i], channel_max[i], 
                adc_raw[i], adc_avg[i], adc_filtered[i], hysteresis[i]);
    }
    printf("======================\n");
}

static int cmd_config(int argc, char **argv) {
    int errors = arg_parse(argc, argv, (void *)&config_args);
    
    if (errors > 0 || config_args.help->count > 0) {
        print_help_config();
        return 0;
    }

    int ch = (config_args.channel->count > 0) ? config_args.channel->ival[0] : -1;
    
    if (ch >= CH_MAX || ch < -1) {
        printf("Invalid channel (must be 0-%d)\n", CH_MAX-1);
        return 1;
    }

    // Read from NVS
    if (config_args.read->count > 0) {
        for (int i = 0; i < CH_MAX; i++) {
            channel_min[i] = nvs_get_channel_i32("ch_min", i, 0);
            channel_max[i] = nvs_get_channel_i32("ch_max", i, 4095);
            hysteresis[i] = nvs_get_channel_i32("ch_hyst", i, 10);
        }
        printf("Loaded configuration from NVS\n");
    }

    // Write to NVS
    if (config_args.write->count > 0) {
        for (int i = 0; i < CH_MAX; i++) {
            nvs_set_channel_i32("ch_min", i, channel_min[i]);
            nvs_set_channel_i32("ch_max", i, channel_max[i]);
            nvs_set_channel_i32("ch_hyst", i, hysteresis[i]);
        }
        printf("Saved configuration to NVS\n");
    }

    // Set values for specific channel
    if (config_args.minval->count > 0 || config_args.maxval->count > 0 || config_args.hyst->count > 0) {
        if (ch == -1) {
            printf("Error: Specify channel with -c when setting values\n");
            return 1;
        }

        bool changed = false;

        if (config_args.minval->count > 0) {
            int val = config_args.minval->ival[0];
            if (val < 0 || val > 4095) {
                printf("Error: min must be 0-4095\n");
                return 1;
            }
            channel_min[ch] = val;
            printf("CH%d min = %d\n", ch, val);
            changed = true;
        }

        if (config_args.maxval->count > 0) {
            int val = config_args.maxval->ival[0];
            if (val < 0 || val > 4095) {
                printf("Error: max must be 0-4095\n");
                return 1;
            }
            channel_max[ch] = val;
            printf("CH%d max = %d\n", ch, val);
            changed = true;
        }

        if (config_args.hyst->count > 0) {
            int val = config_args.hyst->ival[0];
            if (val < 0 || val > 500) {
                printf("Error: hysteresis must be 0-500\n");
                return 1;
            }
            hysteresis[ch] = val;
            printf("CH%d hysteresis = %d\n", ch, val);
            changed = true;
        }

        // Validate and save
        if (channel_min[ch] > channel_max[ch]) {
            printf("Warning: min > max, swapping\n");
            int tmp = channel_min[ch];
            channel_min[ch] = channel_max[ch];
            channel_max[ch] = tmp;
        }

        if (changed) {
            nvs_set_channel_i32("ch_min", ch, channel_min[ch]);
            nvs_set_channel_i32("ch_max", ch, channel_max[ch]);
            nvs_set_channel_i32("ch_hyst", ch, hysteresis[ch]);
            printf("Saved to NVS\n");
        }
    }

    // Show status
    if (config_args.status->count > 0) {
        print_channel_status();
    }

    return 0;
}

/**
 * @brief Register new config command
 */
static void register_config_command(void) {
    config_args.help = arg_litn("h", "help", 0, 1, "Show help");
    config_args.read = arg_litn("r", "read", 0, 1, "Read from NVS");
    config_args.write = arg_litn("w", "write", 0, 1, "Write to NVS");
    config_args.channel = arg_intn("c", "channel", "<ch>", 0, 1, "Channel 0-5");
    config_args.minval = arg_intn("m", "min", "<val>", 0, 1, "Min value");
    config_args.maxval = arg_intn("M", "max", "<val>", 0, 1, "Max value");
    config_args.status = arg_litn("s", "status", 0, 1, "Show status");
    config_args.hyst = arg_intn("y", "hyst", "<val>", 0, 1, "Hysteresis");
    config_args.end = arg_end(20);

    esp_console_cmd_t cmd = {
        .command = CMD_CONFIG,
        .help = "Configure ADC channels",
        .hint = NULL,
        .func = &cmd_config,
        .argtable = &config_args
    };

    esp_console_cmd_register(&cmd);
}

/**
 * @brief Register all CLI commands
 */
void register_commands(void)
{
    esp_console_register_help_command();
    register_config_command();
}

/**
 * @brief CLI task for FreeRTOS
 */
void cli_task(void *arg)
{
    ESP_LOGI("CLI", "cli_task started");
    
    // Small delay to let system stabilize
    vTaskDelay(pdMS_TO_TICKS(500));

    // Configure stdin to be unbuffered
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    // Initialize console with minimal config
    esp_console_config_t console_config = {
        .max_cmdline_length = 256,
        .max_cmdline_args = 16,
        .hint_color = 36,  // Cyan
    };
    
    ESP_ERROR_CHECK(esp_console_init(&console_config));
    
    ESP_LOGI("CLI", "Console initialized");
    
    register_commands();
    
    ESP_LOGI("CLI", "Commands registered");

    // Use linenoise for proper line editing
    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(100);
    linenoiseAllowEmpty(false);

    printf("\n\n");
    printf("Type 'help' to see available commands.\n");
    
    const char* prompt = "CMD> ";
    
    while (true) {
        // Use linenoise for line input with editing support
        char* line = linenoise(prompt);
        
        if (line == NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        // Trim leading/trailing whitespace
    char *trimmed = line;
    while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n') {
        trimmed++;
    }
    
    // Remove trailing whitespace
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    
    // Skip empty lines
    if (strlen(trimmed) == 0) {
        linenoiseFree(line);
        continue;
    }
    
    // Add to history
    linenoiseHistoryAdd(trimmed);
    
    // Run command
    int ret;
    esp_err_t err = esp_console_run(trimmed, &ret);
        
        linenoiseFree(line);
    }
    
    vTaskDelete(NULL);
}