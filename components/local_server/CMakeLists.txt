set(srcs "src/httpd.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS include
                    REQUIRES mdns
                    REQUIRES nvs_flash
                    REQUIRES esp_http_server
                    EMBED_FILES "esp_index.html")