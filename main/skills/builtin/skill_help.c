/**
 * @file skill_help.c
 * @brief 帮助技能实现
 * @author MtZhr
 */

#include "skill_help.h"
#include "../skill_registry.h"
#include "esp_log.h"

static const char *TAG = "skill_help";

static esp_err_t skill_help_execute(const cJSON *params, skill_result_t *result) {
    int count = 0;
    const skill_def_t *skills = skill_list(&count);
    
    int len = snprintf(result->message, sizeof(result->message), 
                      "可用技能 (%d):\n", count);
    
    for (int i = 0; i < count && len < (int)sizeof(result->message) - 100; i++) {
        len += snprintf(result->message + len, sizeof(result->message) - len,
                       "- %s: %s\n", skills[i].name, skills[i].description);
    }
    
    result->success = true;
    return ESP_OK;
}

esp_err_t skill_help_register(void) {
    static const char *aliases[] = {"帮助", "?"};
    
    skill_def_t skill = {
        .name = "help",
        .description = "显示所有可用技能",
        .category = "system",
        .execute = skill_help_execute,
        .aliases = aliases,
        .alias_count = 2
    };
    
    return skill_register(&skill);
}