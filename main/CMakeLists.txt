idf_component_register(
    SRCS
        "litearm.c"
        "bus/message_bus.c"
        "wifi/wifi_manager.c"
        "wifi/wifi_config.c"
        "channels/feishu/feishu_bot.c"
        "agent/command_parser.c"
        "agent/agent_core.c"
        "tools/tool_registry.c"
        "tools/tool_get_time.c"
        "tools/tool_files.c"
        "tools/tool_cron.c"
        "tools/tool_system.c"
        "heartbeat/heartbeat.c"
    INCLUDE_DIRS
        "."
        "bus"
        "wifi"
        "channels/feishu"
        "agent"
        "tools"
        "heartbeat"
    REQUIRES
        nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
        esp_event json spiffs console vfs esp-tls esp_timer esp_websocket_client
        lwip driver
)
