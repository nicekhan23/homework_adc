idf_component_register(SRCS "cli.c" "nvs.c" "adc.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES console driver nvs_flash)