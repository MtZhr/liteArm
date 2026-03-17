/**
 * @file skill_file_ops.c
 * @brief 文件操作技能实现
 * 
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "skill_file_ops.h"
#include "../skill_registry.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "skill_file";

// 读取文件
static esp_err_t skill_read_file(const cJSON *params, skill_result_t *result) {
    cJSON *path_json = cJSON_GetObjectItem(params, "path");
    if (!path_json || !path_json->valuestring) {
        result->success = false;
        snprintf(result->message, sizeof(result->message), "缺少path参数");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *path = path_json->valuestring;
    FILE *f = fopen(path, "r");
    if (!f) {
        result->success = false;
        snprintf(result->message, sizeof(result->message), "无法打开文件: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    char content[1024];
    size_t len = fread(content, 1, sizeof(content) - 1, f);
    content[len] = '\0';
    fclose(f);
    
    result->success = true;
    snprintf(result->message, sizeof(result->message), "文件内容:\n%s", content);
    
    ESP_LOGI(TAG, "Read file: %s (%d bytes)", path, (int)len);
    return ESP_OK;
}

// 写入文件
static esp_err_t skill_write_file(const cJSON *params, skill_result_t *result) {
    cJSON *path_json = cJSON_GetObjectItem(params, "path");
    cJSON *content_json = cJSON_GetObjectItem(params, "content");
    
    if (!path_json || !content_json) {
        result->success = false;
        snprintf(result->message, sizeof(result->message), "缺少必要参数");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *path = path_json->valuestring;
    const char *content = content_json->valuestring;
    
    FILE *f = fopen(path, "w");
    if (!f) {
        result->success = false;
        snprintf(result->message, sizeof(result->message), "无法创建文件: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    size_t len = fwrite(content, 1, strlen(content), f);
    fclose(f);
    
    result->success = true;
    snprintf(result->message, sizeof(result->message), "写入成功: %d 字节", (int)len);
    
    ESP_LOGI(TAG, "Write file: %s (%d bytes)", path, (int)len);
    return ESP_OK;
}

// 执行函数
static esp_err_t skill_file_ops_execute(const cJSON *params, skill_result_t *result) {
    cJSON *action_json = cJSON_GetObjectItem(params, "action");
    const char *action = action_json ? action_json->valuestring : "read";
    
    if (strcmp(action, "read") == 0) {
        return skill_read_file(params, result);
    } else if (strcmp(action, "write") == 0) {
        return skill_write_file(params, result);
    }
    
    result->success = false;
    snprintf(result->message, sizeof(result->message), "未知操作: %s", action);
    return ESP_ERR_INVALID_ARG;
}

// 注册函数
esp_err_t skill_file_ops_register(void) {
    static const char *aliases[] = {"文件", "file"};
    
    static skill_param_def_t params[] = {
        {"action", SKILL_PARAM_STRING, false, "read", "操作类型: read/write"},
        {"path", SKILL_PARAM_STRING, true, NULL, "文件路径"},
        {"content", SKILL_PARAM_STRING, false, NULL, "文件内容（写入时需要）"}
    };
    
    skill_def_t skill = {
        .name = "file_ops",
        .description = "文件操作：读取/写入文件",
        .category = "file",
        .params = params,
        .param_count = 3,
        .execute = skill_file_ops_execute,
        .aliases = aliases,
        .alias_count = 2
    };
    
    return skill_register(&skill);
}