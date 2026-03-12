/**
 * @file tool_get_time.c
 * @brief Get Current Time Tool
 */

#include "tool_registry.h"
#include "litearm_config.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sntp.h"

static const char *TAG = "time";

static bool s_time_synced = false;

/**
 * @brief 时间同步回调
 */
static void time_sync_cb(struct timeval *tv)
{
    s_time_synced = true;
    ESP_LOGI(TAG, "Time synchronized: %lld", (long long)tv->tv_sec);
}

/**
 * @brief 初始化 SNTP
 */
esp_err_t tool_get_time_init(void)
{
    /* 设置时区 */
    setenv("TZ", "CST-8", 1);
    tzset();
    
    /* 配置 SNTP */
    ESP_LOGI(TAG, "Initializing SNTP...");
    
    /* 设置操作模式 */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    /* 设置 NTP 服务器 */
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "ntp.tencent.com");
    
    /* 设置同步模式 */
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    
    /* 设置同步回调 */
    sntp_set_time_sync_notification_cb(time_sync_cb);
    
    /* 初始化 SNTP - esp_sntp_init() 在 ESP-IDF 5.x 中返回 void */
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP initialized successfully");
    
    return ESP_OK;
}

/**
 * @brief 获取当前时间
 */
esp_err_t tool_get_time_execute(const char *params_json, char *output, size_t output_size)
{
    (void)params_json;
    
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    /* 格式化输出 */
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    /* 星期 */
    const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    const char *weekday = weekdays[timeinfo.tm_wday];
    
    snprintf(output, output_size,
        "当前时间: %s %s\n\n"
        "详细:\n"
        "  日期: %04d-%02d-%02d\n"
        "  时间: %02d:%02d:%02d\n"
        "  星期: %s\n"
        "  时间戳: %lld\n"
        "  同步状态: %s",
        time_str, weekday,
        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
        weekday,
        (long long)now,
        s_time_synced ? "已同步" : "未同步");
    
    return ESP_OK;
}
