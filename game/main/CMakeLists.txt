idf_component_register(
    SRCS "app_main.c" "serial_io.c" "lownet.c" "lownet_util.c" "lownet_crypt.c" "app_chat.c" "app_ping.c" "utility.c" "app_command.c" "gameserver.c" "tictactoe.c" "games.c"  "tictac_node.c"
    INCLUDE_DIRS "."
	REQUIRES "esp_wifi" "nvs_flash" "esp_timer" "mbedtls"
)
