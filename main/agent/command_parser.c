/**
 * @file command_parser.c
 * @brief Command Parser Implementation
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "command_parser.h"
#include "skills/skill_registry.h"
#include "litearm_config.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "parser";

/* 静态帮助文本 */
static const char *HELP_TEXT = 
    TXT_HELP_HEADER
    TXT_HELP_FORMAT
    TXT_HELP_AVAILABLE
    TXT_HELP_EXAMPLES;


/**
 * @brief 字符串首尾空白处理
 */
static char *trim(char *str)
{
    if (!str) return str;
    
    /* 首部空白处理 */
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == '\0') return str;
    
    /* 尾部空白处理 */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}


/**
 * @brief 提取 JSON 参数
 * 从 "[...]" 中提取 JSON 字符串
 */
static esp_err_t extract_json_params(const char *input, char *output, size_t output_size)
{
    if (!input || !output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 查找 '[' 和 ']' */
    const char *start = strchr(input, '[');
    const char *end = strchr(input, ']');
    
    if (!start || !end || end <= start) {
        /* 没有找到 JSON 参数 */
        output[0] = '\0';
        return ESP_ERR_NOT_FOUND;
    }
    
    /* 提取 JSON 内容 */
    size_t json_len = (size_t)(end - start - 1);
    if (json_len >= output_size) {
        json_len = output_size - 1;
    }
    
    strncpy(output, start + 1, json_len);
    output[json_len] = '\0';
    
    return ESP_OK;
}

/**
 * @brief 提取空格分隔的参数
 */
static esp_err_t extract_space_params(const char *input, char *output, size_t output_size)
{
    if (!input || !output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 查找第一个空格 */
    const char *space = strchr(input, ' ');
    if (!space) {
        output[0] = '\0';
        return ESP_ERR_NOT_FOUND;
    }
    
    /* 提取空格后的内容 */
    space++;
    while (*space == ' ') space++;  /* 跳过多个空格 */
    
    strncpy(output, space, output_size - 1);
    output[output_size - 1] = '\0';
    
    return ESP_OK;
}

/**
 * @brief 解析技能调用请求
 * 支持格式:
 *   !skill_name
 *   !skill_name[params]
 *   /skill_name
 *   /skill_name params
 *   /skill_name[params]
 */
static esp_err_t parse_skill_call(const char *input, skill_request_t *skill_req)
{
    if (!input || !skill_req) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(skill_req, 0, sizeof(skill_request_t));
    skill_req->is_valid = false;
    
    /* 首部空白处理并复制一份 */
    char *trimmed = trim((char*)input);
    if (!trimmed || strlen(trimmed) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char *input_copy = strdup(trimmed);
    if (!input_copy) {
        return ESP_ERR_NO_MEM;
    }
    
    /* 判断是否为技能调用 */
    char prefix[2] = {0};
    prefix[0] = input_copy[0];
    
    if (prefix[0] != '!' && prefix[0] != '/') {
        free(input_copy);
        return ESP_ERR_NOT_FOUND;  /* 不是技能调用 */
    }
    
    /* 提取技能名 */
    char *name_start = input_copy + 1;
    char *name_end = name_start;
    
    /* 技能名结束位置: 空格、[、或字符串结尾 */
    while (*name_end) {
        if (*name_end == ' ' || *name_end == '[') {
            break;
        }
        name_end++;
    }
    
    size_t name_len = (size_t)(name_end - name_start);
    if (name_len == 0 || name_len >= LITEARM_TOOL_NAME_MAX_LEN) {
        free(input_copy);
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(skill_req->skill_name, name_start, name_len);
    skill_req->skill_name[name_len] = '\0';
    
    /* 检查技能是否存在 */
    if (!skill_find(skill_req->skill_name)) {
        ESP_LOGW(TAG, "Skill not found: %s", skill_req->skill_name);
        free(input_copy);
        return ESP_ERR_NOT_FOUND;
    }
    
    /* 提取参数 */
    char params_buf[LITEARM_TOOL_PARAMS_MAX_LEN] = {0};
    
    if (*name_end == '[') {
        /* JSON 格式: skill_name[params] */
        esp_err_t err = extract_json_params(name_end, params_buf, sizeof(params_buf));
        if (err == ESP_OK && params_buf[0] != '\0') {
            skill_req->params_json = strdup(params_buf);
        }
    } else if (*name_end == ' ') {
        /* 空格格式: skill_name params */
        esp_err_t err = extract_space_params(name_end, params_buf, sizeof(params_buf));
        if (err == ESP_OK && params_buf[0] != '\0') {
            /* 尝试解析为 JSON，如果失败则作为字符串 */
            if (params_buf[0] == '{') {
                skill_req->params_json = strdup(params_buf);
            } else {
                /* 包装为 JSON 对象 */
                size_t needed = strlen(params_buf) + 64;
                skill_req->params_json = malloc(needed);
                if (skill_req->params_json) {
                    snprintf(skill_req->params_json, needed, 
                             "{\"raw\": \"%s\"}", params_buf);
                }
            }
        }
    }
    
    skill_req->is_valid = true;
    free(input_copy);
    
    return ESP_OK;
}

/**
 * @brief 解析飞书消息
 */
esp_err_t parse_feishu_message(const feishu_message_t *msg, parse_result_t *result)
{
    if (!msg || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(parse_result_t));
    
    result->msg_type = msg->type;
    result->is_skill_call = false;
    
    switch (msg->type) {
        case MSG_TYPE_TEXT:
            return parse_command_text(msg->content.text, result);
            
        case MSG_TYPE_IMAGE:
            /* 图片消息 */
            result->raw_text = strdup("TXT_MSG_TYPE_IMAGE");
            break;
            
        case MSG_TYPE_VOICE:
            /* 语音消息 */
            result->raw_text = strdup("TXT_MSG_TYPE_VOICE");
            break;
            
        case MSG_TYPE_FILE:
            /* 文件消息 */
            result->raw_text = strdup("TXT_MSG_TYPE_FILE");
            break;
            
        case MSG_TYPE_CARD:
            /* 卡片消息 - 尝试解析 JSON */
            if (msg->content.text) {
                return parse_command_text(msg->content.text, result);
            }
            result->raw_text = strdup("TXT_MSG_TYPE_CARD");
            break;
            
        default:
            result->raw_text = strdup("TXT_MSG_TYPE_UNKNOWN");
            break;
    }
    
    return ESP_OK;
}

/**
 * @brief 解析文本命令
 */
esp_err_t parse_command_text(const char *text, parse_result_t *result)
{
    if (!text || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(parse_result_t));
    
    /* 复制原始文本 */
    result->raw_text = strdup(text);
    if (!result->raw_text) {
        return ESP_ERR_NO_MEM;
    }
    
    /* 检查是否为帮助命令 */
    char *trimmed = trim((char*)text);
    if (strcasecmp(trimmed, "help") == 0 || 
        strcasecmp(trimmed, "!help") == 0 ||
        strcasecmp(trimmed, "/help") == 0) {
        result->is_skill_call = true;
        strncpy(result->skill.skill_name, "help", sizeof(result->skill.skill_name) - 1);
        result->skill.is_valid = true;
        result->skill.params_json = NULL;
        return ESP_OK;
    }
    
    /* 尝试解析为技能调用 */
    esp_err_t err = parse_skill_call(trimmed, &result->skill);
    if (err == ESP_OK && result->skill.is_valid) {
        result->is_skill_call = true;
        return ESP_OK;
    }
    
    /* 不是有效的技能调用 */
    return ESP_OK;
}

/**
 * @brief 释放解析结果资源
 */
void parse_result_free(parse_result_t *result)
{
    if (!result) return;
    
    if (result->raw_text) {
        free(result->raw_text);
        result->raw_text = NULL;
    }
    
    if (result->skill.params_json) {
        free(result->skill.params_json);
        result->skill.params_json = NULL;
    }
    
    memset(result, 0, sizeof(parse_result_t));
}

/**
 * @brief 获取帮助文本
 */
const char* command_parser_get_help_text(void)
{
    return HELP_TEXT;
}
