/**
 * @file skill_get_time.c
 * @brief 时间技能实现
 * 
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "skill_get_time.h"
#include "../skill_registry.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "skill_time";

// 执行函数
static esp_err_t skill_get_time_execute(const cJSON *params, skill_result_t *result) {
    (void)params;
    
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    
    // 获取当前时间
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // 检查时间是否已同步（年份大于 2020）
    if (timeinfo.tm_year + 1900 < 2020) {
        result->success = false;
        snprintf(result->message, sizeof(result->message), 
                "时间未同步，请检查网络连接");
        ESP_LOGW(TAG, "Time not synchronized");
        return ESP_OK;
    }
    
    // 格式化时间字符串
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    // 设置结果
    result->success = true;
    snprintf(result->message, sizeof(result->message), "当前时间: %s", strftime_buf);
    
    // 添加结构化数据
    result->data = cJSON_CreateObject();
    if (result->data) {
        cJSON_AddStringToObject(result->data, "time", strftime_buf);
        cJSON_AddNumberToObject(result->data, "timestamp", (double)now);
    }
    
    ESP_LOGI(TAG, "Get time: %s", strftime_buf);
    return ESP_OK;
}

// 注册函数
esp_err_t skill_get_time_register(void) {
    static const char *aliases[] = {"时间", "time", "now"};
    
    skill_def_t skill = {
        .name = "get_time",
        .description = "获取当前系统时间",
        .category = "system",
        .params = NULL,
        .param_count = 0,
        .execute = skill_get_time_execute,
        .aliases = aliases,
        .alias_count = 3
    };
    
    return skill_register(&skill);
}
