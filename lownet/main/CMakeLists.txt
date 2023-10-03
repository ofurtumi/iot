idf_component_register(
    SRCS "app_main.c" "serial_io.c" "lownet.c" "lownet_util.c" "app_chat.c" "app_ping.c" "utility.c"
    INCLUDE_DIRS "."
	REQUIRES "esp_wifi" "nvs_flash" "esp_timer"
)
