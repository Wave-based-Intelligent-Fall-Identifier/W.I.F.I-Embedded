idf_component_register(
    SRCS "espnowSTA.c"
    INCLUDE_DIRS "."
    REQUIRES esp_netif nvs_flash 
)