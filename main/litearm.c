/**
 * @file litearm.c
 * @brief liteArm 主入口
 *
 * 基于分层架构的 ESP32 工具调用系统
 * - 内置飞书消息接收/发送
 * - 支持命令格式工具调用
 * - WiFi 配网机制 (物理按钮重置)
 * - 适配 ESP32 2MB Flash
 *
 * @author MtZhr
 * @date 2026-03-12
 * @version 1.0.0
 * @copyright Copyright (c) 2026 MtZhr
 * @github https://github.com/MtZhr/liteArm
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include "litearm_config.h"
#include "bus/message_bus.h"
#include "wifi/wifi_manager.h"
#include "wifi/wifi_config.h"
#include "skills/skill_registry.h"
#include "skills/builtin/skill_get_time.h"
#include "skills/builtin/skill_cron.h"
#include "skills/builtin/skill_file_ops.h"
#include "skills/builtin/skill_system.h"
#include "skills/builtin/skill_help.h"
#include "agent/agent_core.h"
#include "channels/feishu/feishu_bot.h"
#include "heartbeat/heartbeat.h"

static const char *TAG = "litearm";

/**
 * @brief 初始化 NVS
 */
static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief 初始化 SPIFFS
 */
static esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = LITEARM_SPIFFS_BASE,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS: total=%d, used=%d", (int)total, (int)used);

    /* 创建必要目录 */
    mkdir(LITEARM_SPIFFS_CONFIG_DIR, 0755);
    mkdir(LITEARM_SPIFFS_MEMORY_DIR, 0755);
    mkdir(LITEARM_SPIFFS_SESSION_DIR, 0755);

    return ESP_OK;
}

/**
 * @brief 串口 CLI 任务
 */
static void cli_task(void *arg)
{
    (void)arg;
    
    char line[256];
    int pos = 0;
    
    ESP_LOGI(TAG, "CLI ready. Type 'help' for commands.");
    printf("\n> ");
    fflush(stdout);
    
    while (1) {
        int ch = getchar();
        if (ch == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        
        if (ch == '\n' || ch == '\r') {
            if (pos == 0) {
                printf("\n> ");
                fflush(stdout);
                continue;
            }
            
            line[pos] = '\0';
            pos = 0;
            
            printf("\n");
            
            /* 解析命令 */
            if (strcmp(line, "help") == 0) {
                printf("Available Commands:\n\n");
                printf("  help                        - Show this help\n");
                printf("  status                      - Show system status\n");
                printf("  memory                      - Show memory info\n");
                printf("  set_wifi <ssid> <pass>      - Set current WiFi\n");
                printf("  set_initial_wifi <s> <p>    - Set initial WiFi (for reset)\n");
                printf("  reset_wifi                  - Reset to initial WiFi\n");
                printf("  set_feishu <id> <secret>    - Set Feishu credentials\n");
                printf("  heartbeat <cmd> [args]      - Heartbeat commands\n");
                printf("  restart                     - Restart device\n");
                printf("  skills                      - List registered skills\n");
                printf("  scan                        - Scan WiFi networks\n");
            }
            else if (strcmp(line, "status") == 0) {
                printf("System Status:\n\n");
                printf("  WiFi:      %s\n", wifi_manager_is_connected() ? "connected" : "disconnected");
                printf("  SSID:      %s\n", wifi_manager_get_ssid());
                printf("  IP:        %s\n", wifi_manager_get_ip());
                printf("  Initial:   %s\n", wifi_config_is_initial() ? "yes" : "no");
                printf("  Feishu:    %s\n", feishu_is_configured() ? "configured" : "not configured");
                printf("  Heartbeat: %s\n", heartbeat_is_running() ? "running" : "stopped");
                printf("\n");
                printf("  Inbound queue:  %d\n", message_bus_inbound_count());
                printf("  Outbound queue: %d\n", message_bus_outbound_count());
            }
            else if (strcmp(line, "memory") == 0) {
                printf("Memory Info:\n\n");
                printf("  Free heap:     %d bytes\n", 
                       (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
                printf("  Min free heap: %d bytes\n",
                       (int)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
                printf("  Largest block: %d bytes\n",
                       (int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
            }
            else if (strncmp(line, "set_wifi ", 9) == 0) {
                char ssid[64], pass[64];
                if (sscanf(line + 9, "%63s %63s", ssid, pass) == 2) {
                    esp_err_t err = wifi_manager_set_credentials(ssid, pass);
                    if (err == ESP_OK) {
                        printf("WiFi credentials saved. Restart to apply.\n");
                    } else {
                        printf("Failed to save WiFi credentials: %s\n", esp_err_to_name(err));
                    }
                } else {
                    printf("Usage: set_wifi <ssid> <password>\n");
                }
            }
            else if (strncmp(line, "set_initial_wifi ", 17) == 0) {
                char ssid[64], pass[64];
                if (sscanf(line + 17, "%63s %63s", ssid, pass) == 2) {
                    esp_err_t err = wifi_config_set_initial(ssid, pass);
                    if (err == ESP_OK) {
                        printf("Initial WiFi set: %s\n", ssid);
                        printf("Long press reset button (%d ms) to restore.\n", LITEARM_WIFI_RESET_HOLD_MS);
                    } else {
                        printf("Failed: %s\n", esp_err_to_name(err));
                    }
                } else {
                    printf("Usage: set_initial_wifi <ssid> <password>\n");
                }
            }
            else if (strcmp(line, "reset_wifi") == 0) {
                char init_ssid[64], init_pass[64];
                wifi_config_get_initial(init_ssid, init_pass, sizeof(init_ssid), sizeof(init_pass));
                
                if (init_ssid[0] == '\0') {
                    printf("No initial WiFi configured.\n");
                    printf("Use 'set_initial_wifi <ssid> <pass>' first.\n");
                } else {
                    printf("Resetting to initial WiFi: %s\n", init_ssid);
                    esp_err_t err = wifi_config_reset_to_initial();
                    if (err == ESP_OK) {
                        printf("Reset done. Restarting...\n");
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        esp_restart();
                    } else {
                        printf("Reset failed: %s\n", esp_err_to_name(err));
                    }
                }
            }
            else if (strncmp(line, "set_feishu ", 11) == 0) {
                char app_id[64], secret[128];
                if (sscanf(line + 11, "%63s %127s", app_id, secret) == 2) {
                    esp_err_t err = feishu_set_credentials(app_id, secret);
                    if (err == ESP_OK) {
                        printf("Feishu credentials saved.\n");
                    } else {
                        printf("Failed: %s\n", esp_err_to_name(err));
                    }
                } else {
                    printf("Usage: set_feishu <app_id> <app_secret>\n");
                }
            }
            else if (strncmp(line, "heartbeat ", 10) == 0) {
                const char *cmd = line + 10;
                
                if (strcmp(cmd, "status") == 0) {
                    printf("Heartbeat Status:\n");
                    printf("  Running:  %s\n", heartbeat_is_running() ? "yes" : "no");
                }
                else if (strcmp(cmd, "start") == 0) {
                    heartbeat_start();
                    printf("Heartbeat started.\n");
                }
                else if (strcmp(cmd, "stop") == 0) {
                    heartbeat_stop();
                    printf("Heartbeat stopped.\n");
                }
                else if (strcmp(cmd, "now") == 0) {
                    heartbeat_trigger_now();
                    printf("Heartbeat triggered.\n");
                }
                else if (strncmp(cmd, "enable ", 7) == 0) {
                    const char *target = cmd + 7;
                    char channel[32], chat_id[64];
                    if (sscanf(target, "%31[^:]:%63s", channel, chat_id) == 2) {
                        heartbeat_set_target(channel, chat_id);
                        heartbeat_set_enabled(true);
                        heartbeat_start();
                        printf("Heartbeat enabled for %s:%s\n", channel, chat_id);
                    } else {
                        printf("Usage: heartbeat enable <channel>:<chat_id>\n");
                    }
                }
                else if (strncmp(cmd, "interval ", 9) == 0) {
                    uint32_t minutes = atoi(cmd + 9);
                    if (minutes > 0) {
                        heartbeat_set_interval(minutes * 60 * 1000);
                        printf("Heartbeat interval set to %lu minutes\n", (unsigned long)minutes);
                    } else {
                        printf("Usage: heartbeat interval <minutes>\n");
                    }
                }
                else {
                    printf("Heartbeat commands:\n");
                    printf("  status, start, stop, now, enable, interval\n");
                }
            }
            else if (strcmp(line, "skills") == 0) {
                int count = skill_registry_count();
                printf("Registered Skills (%d):\n\n", count);
                
                const skill_def_t *skills = skill_list(&count);
                for (int i = 0; i < count; i++) {
                    printf("  %s: %s\n", skills[i].name, skills[i].description);
                }
            }
            else if (strcmp(line, "restart") == 0) {
                printf("Restarting...\n");
                fflush(stdout);
                esp_restart();
            }
            else if (strcmp(line, "scan") == 0) {
                printf("Scanning WiFi networks...\n");
                wifi_manager_scan_and_print();
            }
            else {
                printf("Unknown command: %s\n", line);
                printf("Type 'help' for available commands.\n");
            }
            
            printf("\n> ");
            fflush(stdout);
        }
        else if (ch == 0x7F || ch == 0x08) {
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        }
        else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = (char)ch;
            printf("%c", ch);
            fflush(stdout);
        }
    }
}

/**
 * @brief 主入口
 */
void app_main(void)
{
    /* 降低日志级别 */
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  liteArm v%s", LITEARM_VERSION_STRING);
    ESP_LOGI(TAG, "  https://github.com/MtZhr/liteArm");
    ESP_LOGI(TAG, "========================================");
    
    /* 打印内存信息 */
    ESP_LOGI(TAG, "Internal free: %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    
    /* Phase 1: 核心基础设施 */
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_spiffs());
    
    /* Phase 2: 初始化子系统 */
    ESP_ERROR_CHECK(message_bus_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(wifi_config_init());
    ESP_ERROR_CHECK(skill_registry_init());
    ESP_ERROR_CHECK(skill_get_time_register());
    ESP_ERROR_CHECK(skill_file_ops_register());
    ESP_ERROR_CHECK(skill_cron_register());
    ESP_ERROR_CHECK(skill_system_register());
    ESP_ERROR_CHECK(skill_help_register());
    ESP_ERROR_CHECK(agent_core_init());
    ESP_ERROR_CHECK(feishu_bot_init());
    ESP_ERROR_CHECK(heartbeat_init());
    
    /* Phase 3: 配置 WiFi 重置按钮 */
    ESP_LOGI(TAG, "Configuring WiFi reset button (GPIO=%d)...", LITEARM_WIFI_RESET_GPIO);
    wifi_config_set_reset_button(LITEARM_WIFI_RESET_GPIO, 
                                  LITEARM_WIFI_RESET_ACTIVE_LOW, 
                                  LITEARM_WIFI_RESET_HOLD_MS);
    
    /* 配置状态 LED (如果启用) */
    if (LITEARM_WIFI_LED_GPIO >= 0) {
        wifi_config_enable_status_led(LITEARM_WIFI_LED_GPIO, LITEARM_WIFI_LED_ACTIVE_LOW);
    }
    
    /* Phase 4: 启动 CLI */
    xTaskCreatePinnedToCore(cli_task, "cli", LITEARM_CLI_STACK, NULL, 
                            LITEARM_CLI_PRIO, NULL, LITEARM_CLI_CORE);
    
    /* Phase 5: 连接 WiFi */
    ESP_LOGI(TAG, "Starting WiFi...");
    esp_err_t wifi_err = wifi_manager_start();
    if (wifi_err == ESP_OK) {
        wifi_manager_scan_and_print();
        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        wifi_err = wifi_manager_wait_connected(30000);
    }
    
    if (wifi_err != ESP_OK) {
        ESP_LOGW(TAG, "WiFi not connected, some features unavailable");
        ESP_LOGW(TAG, "Long press BOOT button (%d ms) to reset WiFi", LITEARM_WIFI_RESET_HOLD_MS);
    } else {
        ESP_LOGI(TAG, "WiFi connected: %s (%s)", wifi_manager_get_ssid(), wifi_manager_get_ip());
    }
    
    /* Phase 6: 启动服务 */
    ESP_ERROR_CHECK(agent_core_start());
    
    if (wifi_err == ESP_OK) {
        ESP_ERROR_CHECK(feishu_bot_start());
    }
    
    /* Phase 7: 启动心跳 */
    heartbeat_start();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "liteArm ready!");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Usage:");
    ESP_LOGI(TAG, "  - Send !help in Feishu for commands");
    ESP_LOGI(TAG, "  - Long press BOOT button to reset WiFi");
    ESP_LOGI(TAG, "========================================");
}
