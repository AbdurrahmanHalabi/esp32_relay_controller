set(srcs "src/wifi_ap.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS include
                    REQUIRES nvs_flash)