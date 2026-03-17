/**
 * @file skill_cron.c
 * @brief 定时任务技能实现
 * 
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "skill_cron.h"
#include "../skill_registry.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "skill_cron";

// 执行函数
static esp_err_t skill_cron_execute(const cJSON *params, skill_result_t *result) {
    cJSON *action_json = cJSON_GetObjectItem(params, "action");
    const char *action = action_json ? action_json->valuestring : "list";
    
    if (strcmp(action, "list") == 0) {
        result->success = true;
        snprintf(result->message, sizeof(result->message), "定时任务列表功能（待实现）");
        return ESP_OK;
    }
    
    result->success = false;
    snprintf(result->message, sizeof(result->message), "未知操作: %s", action);
    return ESP_ERR_INVALID_ARG;
}

// 注册函数
esp_err_t skill_cron_register(void) {
    static const char *aliases[] = {"定时", "cron", "schedule"};
    
    skill_def_t skill = {
        .name = "cron",
        .description = "定时任务管理",
        .category = "system",
        .params = NULL,
        .param_count = 0,
        .execute = skill_cron_execute,
        .aliases = aliases,
        .alias_count = 3
    };
    
    return skill_register(&skill);
}