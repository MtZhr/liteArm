#include "litearm_text.h"
/**
 * @file skill_system.c
 * @brief 系统信息技能实现
 * 
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "skill_system.h"
#include "../skill_registry.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "litearm_config.h"

static const char *TAG = "skill_sys";

static esp_err_t skill_system_execute(const cJSON *params, skill_result_t *result) {
    (void)params;
    
    int free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    
    result->success = true;
    snprintf(result->message, sizeof(result->message),
        TXT_MSG_SYSTEM_STATUS,
        LITEARM_VERSION_STRING,
        free_heap,
        min_free_heap);
    
    result->data = cJSON_CreateObject();
    if (result->data) {
        cJSON_AddStringToObject(result->data, "version", LITEARM_VERSION_STRING);
        cJSON_AddNumberToObject(result->data, "free_heap", free_heap);
        cJSON_AddNumberToObject(result->data, "min_free_heap", min_free_heap);
    }
    
    ESP_LOGI(TAG, "System status: v%s, free_heap=%d", LITEARM_VERSION_STRING, free_heap);
    return ESP_OK;
}

esp_err_t skill_system_register(void) {
    static const char *aliases[] = {"系统", "system", "状态"};
    
    skill_def_t skill = {
        .name = "get_status",
        .description = TXT_SKILL_SYSTEM,
        .category = "system",
        .params = NULL,
        .param_count = 0,
        .execute = skill_system_execute,
        .aliases = aliases,
        .alias_count = 3
    };
    
    return skill_register(&skill);
}