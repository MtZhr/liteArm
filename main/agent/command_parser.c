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
    "=== liteArm 帮助 ===\n"
    "\n"
    "命令格式:\n"
    "  !工具名              - 执行无参数工具\n"
    "  !工具名[参数]        - 执行带参数工具\n"
    "  /工具名 参数         - 空格分隔格式\n"
    "\n"
    "可用工具:\n"
    "  get_time        - 获取当前日期和时间\n"
    "  read_file       - 读取文件内容\n"
    "  write_file      - 写入文件内容\n"
    "  edit_file       - 编辑文件内容\n"
    "  list_dir        - 列出目录文件\n"
    "  cron_add        - 添加定时任务\n"
    "  cron_list       - 列出定时任务\n"
    "  cron_remove     - 删除定时任务\n"
    "\n"
    "示例:\n"
    "  !get_time\n"
    "  !read_file[{\"path\": \"/spiffs/config.txt\"}]\n"
    "  !write_file[{\"path\": \"/spiffs/test.txt\", \"content\": \"hello\"}]\n";

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
 * @brief 检查字符串是否以指定前缀开头
 */
static bool starts_with(const char *str, const char *prefix)
{
    if (!str || !prefix) return false;
    return strncmp(str, prefix, strlen(prefix)) == 0;
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
 * @brief 解析工具调用请求
 * 支持格式:
 *   !tool_name
 *   !tool_name[params]
 *   /tool_name
 *   /tool_name params
 *   /tool_name[params]
 */
static esp_err_t parse_tool_call(const char *input, tool_request_t *tool_req)
{
    if (!input || !tool_req) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(tool_req, 0, sizeof(tool_request_t));
    tool_req->is_valid = false;
    
    /* 首部空白处理并复制一份 */
    char *trimmed = trim((char*)input);
    if (!trimmed || strlen(trimmed) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char *input_copy = strdup(trimmed);
    if (!input_copy) {
        return ESP_ERR_NO_MEM;
    }
    
    /* 判断是否为工具调用 */
    char prefix[2] = {0};
    prefix[0] = input_copy[0];
    
    if (prefix[0] != '!' && prefix[0] != '/') {
        free(input_copy);
        return ESP_ERR_NOT_FOUND;  /* 不是工具调用 */
    }
    
    /* 提取工具名 */
    char *name_start = input_copy + 1;
    char *name_end = name_start;
    
    /* 工具名结束位置: 空格、[、或字符串结尾 */
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
    
    strncpy(tool_req->tool_name, name_start, name_len);
    tool_req->tool_name[name_len] = '\0';
    
    /* 检查工具是否存在 */
    if (!skill_find(tool_req->tool_name)) {
        ESP_LOGW(TAG, "Tool not found: %s", tool_req->tool_name);
        free(input_copy);
        return ESP_ERR_NOT_FOUND;
    }
    
    /* 提取参数 */
    char params_buf[LITEARM_TOOL_PARAMS_MAX_LEN] = {0};
    
    if (*name_end == '[') {
        /* JSON 格式: tool_name[params] */
        esp_err_t err = extract_json_params(name_end, params_buf, sizeof(params_buf));
        if (err == ESP_OK && params_buf[0] != '\0') {
            tool_req->params_json = strdup(params_buf);
        }
    } else if (*name_end == ' ') {
        /* 空格格式: tool_name params */
        esp_err_t err = extract_space_params(name_end, params_buf, sizeof(params_buf));
        if (err == ESP_OK && params_buf[0] != '\0') {
            /* 尝试解析为 JSON，如果失败则作为字符串 */
            if (params_buf[0] == '{') {
                tool_req->params_json = strdup(params_buf);
            } else {
                /* 包装为 JSON 对象 */
                size_t needed = strlen(params_buf) + 64;
                tool_req->params_json = malloc(needed);
                if (tool_req->params_json) {
                    snprintf(tool_req->params_json, needed, 
                             "{\"raw\": \"%s\"}", params_buf);
                }
            }
        }
    }
    
    tool_req->is_valid = true;
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
    result->is_tool_call = false;
    
    switch (msg->type) {
        case MSG_TYPE_TEXT:
            return parse_command_text(msg->content.text, result);
            
        case MSG_TYPE_IMAGE:
            /* 图片消息 */
            result->raw_text = strdup("[图片消息]");
            break;
            
        case MSG_TYPE_VOICE:
            /* 语音消息 */
            result->raw_text = strdup("[语音消息]");
            break;
            
        case MSG_TYPE_FILE:
            /* 文件消息 */
            result->raw_text = strdup("[文件消息]");
            break;
            
        case MSG_TYPE_CARD:
            /* 卡片消息 - 尝试解析 JSON */
            if (msg->content.text) {
                return parse_command_text(msg->content.text, result);
            }
            result->raw_text = strdup("[卡片消息]");
            break;
            
        default:
            result->raw_text = strdup("[未知消息类型]");
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
        result->is_tool_call = true;
        strncpy(result->tool.tool_name, "help", sizeof(result->tool.tool_name) - 1);
        result->tool.is_valid = true;
        result->tool.params_json = NULL;
        return ESP_OK;
    }
    
    /* 尝试解析为工具调用 */
    esp_err_t err = parse_tool_call(trimmed, &result->tool);
    if (err == ESP_OK && result->tool.is_valid) {
        result->is_tool_call = true;
        return ESP_OK;
    }
    
    /* 不是有效的工具调用 */
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
    
    if (result->tool.params_json) {
        free(result->tool.params_json);
        result->tool.params_json = NULL;
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
