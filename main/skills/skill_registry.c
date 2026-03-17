/**
 * @file skill_registry.c
 * @brief 技能注册表实现
 * 
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "skill_registry.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "skill_registry";

// 技能注册表全局实例
static skill_registry_t g_registry = {0};

esp_err_t skill_registry_init(void) {
    if (g_registry.skills != NULL) {
        ESP_LOGW(TAG, "Registry already initialized");
        return ESP_OK;
    }
    
    g_registry.capacity = 16;
    g_registry.skills = malloc(g_registry.capacity * sizeof(skill_def_t));
    if (!g_registry.skills) {
        ESP_LOGE(TAG, "Failed to allocate registry");
        return ESP_ERR_NO_MEM;
    }
    
    g_registry.count = 0;
    ESP_LOGI(TAG, "Skill registry initialized (capacity: %d)", g_registry.capacity);
    return ESP_OK;
}

esp_err_t skill_register(const skill_def_t *skill) {
    if (!skill || !skill->name || !skill->execute) {
        ESP_LOGE(TAG, "Invalid skill definition");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 检查是否已存在
    if (skill_find(skill->name) != NULL) {
        ESP_LOGW(TAG, "Skill '%s' already registered", skill->name);
        return ESP_OK;
    }
    
    // 扩容检查
    if (g_registry.count >= g_registry.capacity) {
        int new_capacity = g_registry.capacity * 2;
        skill_def_t *new_skills = realloc(g_registry.skills, 
                                         new_capacity * sizeof(skill_def_t));
        if (!new_skills) {
            ESP_LOGE(TAG, "Failed to expand registry");
            return ESP_ERR_NO_MEM;
        }
        g_registry.skills = new_skills;
        g_registry.capacity = new_capacity;
        ESP_LOGI(TAG, "Registry expanded to %d", new_capacity);
    }
    
    // 添加技能
    g_registry.skills[g_registry.count] = *skill;
    g_registry.count++;
    
    ESP_LOGI(TAG, "Skill registered: %s (total: %d)", skill->name, g_registry.count);
    return ESP_OK;
}

const skill_def_t* skill_find(const char *name) {
    if (!name || g_registry.count == 0) {
        return NULL;
    }
    
    // 查找技能名称
    for (int i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.skills[i].name, name) == 0) {
            return &g_registry.skills[i];
        }
        
        // 检查别名
        if (g_registry.skills[i].aliases) {
            for (int j = 0; j < g_registry.skills[i].alias_count; j++) {
                if (strcmp(g_registry.skills[i].aliases[j], name) == 0) {
                    return &g_registry.skills[i];
                }
            }
        }
    }
    
    return NULL;
}

const skill_def_t* skill_list(int *count) {
    if (count) {
        *count = g_registry.count;
    }
    return g_registry.skills;
}

int skill_registry_count(void) {
    return g_registry.count;
}

void skill_registry_cleanup(void) {
    if (g_registry.skills) {
        free(g_registry.skills);
        g_registry.skills = NULL;
    }
    g_registry.count = 0;
    g_registry.capacity = 0;
    ESP_LOGI(TAG, "Skill registry cleaned up");
}
