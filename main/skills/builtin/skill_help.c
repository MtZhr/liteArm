#include "litearm_text.h"
/**
 * @file skill_help.c
 * @brief 帮助技能实现
 * @author MtZhr
 */

#include "skill_help.h"
#include "../skill_registry.h"
#include "esp_log.h"


static esp_err_t skill_help_execute(const cJSON *params, skill_result_t *result) {
    int count = 0;
    const skill_def_t *skills = skill_list(&count);
    
    int len = snprintf(result->message, sizeof(result->message), 
                      TXT_HELP_HEADER, count);
    
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
        .description = TXT_SKILL_HELP,
        .category = "system",
        .execute = skill_help_execute,
        .aliases = aliases,
        .alias_count = 2
    };
    
    return skill_register(&skill);
}