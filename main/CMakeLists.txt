idf_component_register(
    SRCS
        "litearm.c"
        "bus/message_bus.c"
        "wifi/wifi_manager.c"
        "wifi/wifi_config.c"
        "channels/feishu/feishu_bot.c"
        "agent/command_parser.c"
        "agent/agent_core.c"
        "skills/skill_registry.c"
        "skills/builtin/skill_get_time.c"
        "skills/builtin/skill_file_ops.c"
        "skills/builtin/skill_cron.c"
        "skills/builtin/skill_system.c"
        "skills/builtin/skill_help.c"
        "heartbeat/heartbeat.c"
    INCLUDE_DIRS
        "."
        "bus"
        "wifi"
        "channels/feishu"
        "agent"
        "skills"
        "skills/builtin"
        "heartbeat"
    REQUIRES
        nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
        esp_event json spiffs console vfs esp-tls esp_timer esp_websocket_client
        lwip driver
)
