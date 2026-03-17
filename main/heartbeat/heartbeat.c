/**
 * @file heartbeat.c
 * @brief Heartbeat Implementation
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "heartbeat.h"
#include "litearm_config.h"
#include "bus/message_bus.h"
#include "wifi/wifi_manager.h"
#include "tools/tool_registry.h"

#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs.h"

static const char *TAG = "heartbeat";

/* 默认心跳间隔 (30分钟) */
#define HEARTBEAT_DEFAULT_INTERVAL_MS   (30 * 60 * 1000)

/* 最小心跳间隔 (1分钟) */
#define HEARTBEAT_MIN_INTERVAL_MS       (60 * 1000)

/* 心跳任务栈大小 */
#define HEARTBEAT_TASK_STACK            (4 * 1024)

/* 心跳任务优先级 */
#define HEARTBEAT_TASK_PRIO             3

/* 心跳配置 */
typedef struct {
    uint32_t interval_ms;       /* 心跳间隔 */
    char channel[32];           /* 目标渠道 */
    char chat_id[64];           /* 目标会话 */
    bool enabled;               /* 是否启用 */
    bool send_on_start;         /* 启动时是否发送 */
} heartbeat_config_t;

/* 状态 */
static heartbeat_config_t s_config = {
    .interval_ms = HEARTBEAT_DEFAULT_INTERVAL_MS,
    .channel = LITEARM_CHAN_SYSTEM,
    .chat_id = "",
    .enabled = false,
    .send_on_start = true,
};

static TaskHandle_t s_heartbeat_task = NULL;
static bool s_running = false;
static bool s_trigger_now = false;

/* 统计信息 */
static uint32_t s_heartbeat_count = 0;
static int64_t s_last_heartbeat_time = 0;

/**
 * @brief 从 NVS 加载配置
 */
static void load_config_from_nvs(void)
{
    nvs_handle_t nvs;
    if (nvs_open("heartbeat", NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }

    uint32_t interval = 0;
    if (nvs_get_u32(nvs, "interval", &interval) == ESP_OK) {
        if (interval >= HEARTBEAT_MIN_INTERVAL_MS) {
            s_config.interval_ms = interval;
        }
    }

    size_t len;
    char buf[64];

    len = sizeof(s_config.channel);
    if (nvs_get_str(nvs, "channel", s_config.channel, &len) != ESP_OK) {
        s_config.channel[0] = '\0';
    }

    len = sizeof(s_config.chat_id);
    if (nvs_get_str(nvs, "chat_id", s_config.chat_id, &len) != ESP_OK) {
        s_config.chat_id[0] = '\0';
    }

    uint8_t enabled = 0;
    if (nvs_get_u8(nvs, "enabled", &enabled) == ESP_OK) {
        s_config.enabled = (enabled != 0);
    }

    nvs_close(nvs);

    ESP_LOGI(TAG, "Config loaded: interval=%lums, enabled=%d, channel=%s",
             (unsigned long)s_config.interval_ms, s_config.enabled, s_config.channel);
}

/**
 * @brief 保存配置到 NVS
 */
static void save_config_to_nvs(void)
{
    nvs_handle_t nvs;
    if (nvs_open("heartbeat", NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }

    nvs_set_u32(nvs, "interval", s_config.interval_ms);
    nvs_set_str(nvs, "channel", s_config.channel);
    nvs_set_str(nvs, "chat_id", s_config.chat_id);
    nvs_set_u8(nvs, "enabled", s_config.enabled ? 1 : 0);
    nvs_commit(nvs);
    nvs_close(nvs);
}

/**
 * @brief 构建心跳消息内容
 */
static char *build_heartbeat_message(void)
{
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    /* 获取系统信息 */
    uint32_t free_heap = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t min_heap = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    const char *ip = wifi_manager_get_ip();
    const char *ssid = wifi_manager_get_ssid();
    bool wifi_ok = wifi_manager_is_connected();

    /* 构建消息 */
    char *msg = malloc(1024);
    if (!msg) return NULL;

    int len = snprintf(msg, 1024,
        "💓 liteArm 心跳\n\n"
        "时间: %s\n"
        "心跳次数: %lu\n"
        "\n"
        "📊 系统状态:\n"
        "  空闲内存: %lu bytes\n"
        "  最小内存: %lu bytes\n"
        "\n"
        "📶 网络状态:\n"
        "  WiFi: %s\n"
        "  SSID: %s\n"
        "  IP: %s\n",
        time_str,
        (unsigned long)s_heartbeat_count,
        (unsigned long)free_heap,
        (unsigned long)min_heap,
        wifi_ok ? "已连接" : "未连接",
        ssid[0] ? ssid : "-",
        ip);

    (void)len;
    return msg;
}

/**
 * @brief 心跳任务
 */
static void heartbeat_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Heartbeat task started (interval: %lu ms)", 
             (unsigned long)s_config.interval_ms);

    /* 启动时发送一次心跳 */
    if (s_config.send_on_start) {
        s_trigger_now = true;
    }

    int64_t last_check = esp_timer_get_time();

    while (s_running) {
        int64_t now = esp_timer_get_time();
        int64_t elapsed_ms = (now - last_check) / 1000;

        /* 检查是否需要发送心跳 */
        bool should_send = s_trigger_now;

        if (!should_send && s_config.enabled && s_config.chat_id[0] != '\0') {
            if ((uint32_t)elapsed_ms >= s_config.interval_ms) {
                should_send = true;
            }
        }

        if (should_send && s_config.enabled && s_config.chat_id[0] != '\0') {
            /* 构建心跳消息 */
            char *msg = build_heartbeat_message();
            if (msg) {
                /* 推送到消息总线 */
                litearm_msg_t out = {0};
                strncpy(out.channel, s_config.channel, sizeof(out.channel) - 1);
                strncpy(out.chat_id, s_config.chat_id, sizeof(out.chat_id) - 1);
                out.content = msg;

                esp_err_t err = message_bus_push_outbound(&out);
                if (err == ESP_OK) {
                    s_heartbeat_count++;
                    s_last_heartbeat_time = now;
                    ESP_LOGI(TAG, "Heartbeat #%lu sent to %s:%s",
                             (unsigned long)s_heartbeat_count,
                             s_config.channel, s_config.chat_id);
                } else {
                    ESP_LOGW(TAG, "Failed to queue heartbeat message");
                    free(msg);
                }
            }

            last_check = now;
            s_trigger_now = false;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Heartbeat task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief 初始化心跳模块
 */
esp_err_t heartbeat_init(void)
{
    /* 从 NVS 加载配置 */
    load_config_from_nvs();

    ESP_LOGI(TAG, "Heartbeat initialized");
    return ESP_OK;
}

/**
 * @brief 启动心跳任务
 */
esp_err_t heartbeat_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "Heartbeat already running");
        return ESP_OK;
    }

    s_running = true;

    BaseType_t ret = xTaskCreatePinnedToCore(
        heartbeat_task,
        "heartbeat",
        HEARTBEAT_TASK_STACK,
        NULL,
        HEARTBEAT_TASK_PRIO,
        &s_heartbeat_task,
        0);

    if (ret != pdPASS) {
        s_running = false;
        ESP_LOGE(TAG, "Failed to create heartbeat task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Heartbeat started");
    return ESP_OK;
}

/**
 * @brief 停止心跳任务
 */
void heartbeat_stop(void)
{
    s_running = false;
    if (s_heartbeat_task) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_heartbeat_task = NULL;
    }
    ESP_LOGI(TAG, "Heartbeat stopped");
}

/**
 * @brief 设置心跳间隔
 */
void heartbeat_set_interval(uint32_t interval_ms)
{
    if (interval_ms < HEARTBEAT_MIN_INTERVAL_MS) {
        interval_ms = HEARTBEAT_MIN_INTERVAL_MS;
    }
    s_config.interval_ms = interval_ms;
    save_config_to_nvs();
    ESP_LOGI(TAG, "Interval set to %lu ms", (unsigned long)interval_ms);
}

/**
 * @brief 设置心跳目标
 */
esp_err_t heartbeat_set_target(const char *channel, const char *chat_id)
{
    if (!channel || !chat_id) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_config.channel, channel, sizeof(s_config.channel) - 1);
    strncpy(s_config.chat_id, chat_id, sizeof(s_config.chat_id) - 1);
    save_config_to_nvs();

    ESP_LOGI(TAG, "Target set to %s:%s", channel, chat_id);
    return ESP_OK;
}

/**
 * @brief 启用/禁用心跳
 */
void heartbeat_set_enabled(bool enabled)
{
    s_config.enabled = enabled;
    save_config_to_nvs();
    ESP_LOGI(TAG, "Heartbeat %s", enabled ? "enabled" : "disabled");
}

/**
 * @brief 检查心跳是否运行
 */
bool heartbeat_is_running(void)
{
    return s_running;
}

/**
 * @brief 触发立即心跳
 */
void heartbeat_trigger_now(void)
{
    s_trigger_now = true;
    ESP_LOGI(TAG, "Heartbeat triggered");
}
