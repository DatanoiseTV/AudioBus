idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES audiobus esp_eth esp_event esp_netif esp_timer
)
