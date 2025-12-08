#include "cli.h"
#include "adc.h"
#include "nvs.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int channel_min[CH_MAX] = {0, 0, 0, 0, 0, 0};
int channel_max[CH_MAX] = {4095, 4095, 4095, 4095, 4095, 4095};

/**
 * @brief Initialize console
 */
static void cli_init(void)
{
    esp_console_config_t console_config = {
        .max_cmdline_length = 128,
        .max_cmdline_args = 8,
    };
    esp_console_init(&console_config);

    register_commands();
}

/**
 * @brief Configure ADC channel settings via CLI
 */
static int config_cmd(int argc, char **argv)
{
    // argtable3 structs
    struct arg_lit  *help    = arg_lit0("h", "help", "Show help");
    struct arg_lit  *read    = arg_lit0("r", "read", "Read from NVS");
    struct arg_lit  *write   = arg_lit0("w", "write", "Write to NVS");
    struct arg_int  *channel = arg_int0("c", "channel", "<ch>", "Channel number 0..5");
    struct arg_int  *minval  = arg_int0("m", "min", "<val>", "Minimum value for channel");
    struct arg_int  *maxval  = arg_int0("M", "max", "<val>", "Maximum value for channel");
    struct arg_lit  *start   = arg_lit0("s", "start", "Show previous state/start CLI info");
    struct arg_end  *end     = arg_end(20);

    void* argtable[] = {help, read, write, channel, minval, maxval, start, end};

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors != 0 || help->count > 0) {
        printf("Usage: config [-h] [-r] [-w] [-c <ch>] [-m <min>] [-M <max>] [-s]\n");
        arg_print_errors(stdout, end, "config");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 1;
    }

    int ch = (channel->count > 0) ? channel->ival[0] : -1;
    if (ch >= CH_MAX || ch < -1) {
        printf("Invalid channel\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 1;
    }

    // Read from NVS
    if (read->count > 0) {
        for (int i = 0; i < CH_MAX; i++) {
            channel_min[i] = nvs_get_channel_i32("ch_min", i, 0);
            channel_max[i] = nvs_get_channel_i32("ch_max", i, 4095);
        }
        printf("Loaded min/max from NVS.\n");
    }

    // Write to NVS
    if (write->count > 0) {
        for (int i = 0; i < CH_MAX; i++) {
            nvs_set_channel_i32("ch_min", i, channel_min[i]);
            nvs_set_channel_i32("ch_max", i, channel_max[i]);
        }
        printf("Saved min/max to NVS.\n");
    }

    // Set min/max for a specific channel
    if (ch != -1) {
        if (minval->count > 0) {
            int val = minval->ival[0];
            if (val < 0 || val > 4095) { printf("Invalid min value\n"); arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0])); return 1;}
            channel_min[ch] = val;
            printf("CH%d min set to %d\n", ch, val);
        }
        if (maxval->count > 0) {
            int val = maxval->ival[0];
            if (val < 0 || val > 4095) { printf("Invalid max value\n"); arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0])); return 1;}
            channel_max[ch] = val;
            printf("CH%d max set to %d\n", ch, val);
        }
        if (minval->count > 0 && maxval->count > 0 && channel_min[ch] > channel_max[ch]) {
            printf("Warning: min > max, swapping values\n");
            int tmp = channel_min[ch]; channel_min[ch] = channel_max[ch]; channel_max[ch] = tmp;
        }
    }

    // Start info
    if (start->count > 0) {
        for (int i = 0; i < CH_MAX; i++) {
            printf("CH%d: min=%d max=%d filtered=%d\n", i, channel_min[i], channel_max[i], adc_filtered[i]);
        }
    }

    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    return 0;
}

/**
 * @brief Register new config command
 */
void register_config_command(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "config",
        .help = "Configure channels: min/max, read/write NVS, start info",
        .hint = "[-h] [-r] [-w] [-c <ch>] [-m <min>] [-M <max>] [-s]",
        .func = &config_cmd
    });
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
    cli_init();
    
    printf("\nESP32 ADC CLI ready\n");
    printf("Type 'help' for available commands\n\n");
    
    // Start the console REPL
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "CMD> ";
    repl_config.max_cmdline_length = 128;
    
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
