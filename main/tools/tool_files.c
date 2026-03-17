/**
 * @file tool_files.c
 * @brief File Operations Tools
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "tool_registry.h"
#include "litearm_config.h"

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "files";

/* 最大文件读取大小 */
#define MAX_FILE_READ_SIZE  (4 * 1024)

/**
 * @brief 解析 JSON 参数中的字符串字段
 */
static const char* json_get_string(cJSON *obj, const char *key)
{
    if (!obj || !key) return NULL;
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsString(item)) {
        return item->valuestring;
    }
    return NULL;
}

/**
 * @brief 验证路径是否在 SPIFFS 范围内
 */
static bool is_valid_path(const char *path)
{
    if (!path) return false;
    
    /* 必须以 /spiffs 开头 */
    if (strncmp(path, LITEARM_SPIFFS_BASE, strlen(LITEARM_SPIFFS_BASE)) != 0) {
        return false;
    }
    
    /* 禁止 .. 路径遍历 */
    if (strstr(path, "..") != NULL) {
        return false;
    }
    
    return true;
}

/**
 * @brief 读取文件
 */
esp_err_t tool_read_file_execute(const char *params_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 解析参数 */
    cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
    if (!params) {
        snprintf(output, output_size, "错误: 参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *path = json_get_string(params, "path");
    if (!path) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 缺少 path 参数");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 验证路径 */
    if (!is_valid_path(path)) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 无效路径 (必须以 /spiffs 开头)");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 打开文件 */
    FILE *f = fopen(path, "r");
    if (!f) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 文件不存在或无法打开: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    /* 读取文件内容 */
    size_t total = 0;
    int ch;
    while ((ch = fgetc(f)) != EOF && total < output_size - 1) {
        output[total++] = (char)ch;
    }
    output[total] = '\0';
    
    fclose(f);
    cJSON_Delete(params);
    
    ESP_LOGI(TAG, "Read %d bytes from %s", (int)total, path);
    return ESP_OK;
}

/**
 * @brief 写入文件
 */
esp_err_t tool_write_file_execute(const char *params_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 解析参数 */
    cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
    if (!params) {
        snprintf(output, output_size, "错误: 参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *path = json_get_string(params, "path");
    const char *content = json_get_string(params, "content");
    
    if (!path) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 缺少 path 参数");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!content) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 缺少 content 参数");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 验证路径 */
    if (!is_valid_path(path)) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 无效路径 (必须以 /spiffs 开头)");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 创建目录 (如果需要) */
    char dir_path[256];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash && last_slash != dir_path) {
        *last_slash = '\0';
        mkdir(dir_path, 0755);
    }
    
    /* 打开文件 */
    FILE *f = fopen(path, "w");
    if (!f) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 无法创建文件: %s", path);
        return ESP_ERR_INVALID_STATE;
    }
    
    /* 写入内容 */
    size_t content_len = strlen(content);
    size_t written = fwrite(content, 1, content_len, f);
    fclose(f);
    cJSON_Delete(params);
    
    if (written != content_len) {
        snprintf(output, output_size, "警告: 写入不完整 (%d/%d bytes)", (int)written, (int)content_len);
        return ESP_ERR_INVALID_STATE;
    }
    
    snprintf(output, output_size, "文件写入成功: %s (%d bytes)", path, (int)written);
    ESP_LOGI(TAG, "Wrote %d bytes to %s", (int)written, path);
    return ESP_OK;
}

/**
 * @brief 编辑文件 (查找替换)
 */
esp_err_t tool_edit_file_execute(const char *params_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 解析参数 */
    cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
    if (!params) {
        snprintf(output, output_size, "错误: 参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *path = json_get_string(params, "path");
    const char *old_str = json_get_string(params, "old_string");
    const char *new_str = json_get_string(params, "new_string");
    
    if (!path || !old_str || !new_str) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 缺少必要参数 (path, old_string, new_string)");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 验证路径 */
    if (!is_valid_path(path)) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 无效路径");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 读取文件 */
    FILE *f = fopen(path, "r");
    if (!f) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 文件不存在: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    char *content = malloc(MAX_FILE_READ_SIZE);
    if (!content) {
        fclose(f);
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 内存不足");
        return ESP_ERR_NO_MEM;
    }
    
    size_t len = fread(content, 1, MAX_FILE_READ_SIZE - 1, f);
    content[len] = '\0';
    fclose(f);
    
    /* 查找并替换 */
    char *pos = strstr(content, old_str);
    if (!pos) {
        free(content);
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 未找到要替换的内容");
        return ESP_ERR_NOT_FOUND;
    }
    
    /* 执行替换 (仅替换第一个匹配) */
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    
    /* 计算新长度 */
    size_t new_content_len = len - old_len + new_len;
    if (new_content_len >= MAX_FILE_READ_SIZE) {
        free(content);
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 替换后内容超出限制");
        return ESP_ERR_INVALID_SIZE;
    }
    
    /* 移动剩余内容 */
    memmove(pos + new_len, pos + old_len, len - (pos - content) - old_len + 1);
    
    /* 插入新内容 */
    memcpy(pos, new_str, new_len);
    
    /* 写回文件 */
    f = fopen(path, "w");
    if (!f) {
        free(content);
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 无法写入文件");
        return ESP_ERR_INVALID_STATE;
    }
    
    fwrite(content, 1, new_content_len, f);
    fclose(f);
    
    free(content);
    cJSON_Delete(params);
    
    snprintf(output, output_size, "文件编辑成功: %s", path);
    ESP_LOGI(TAG, "Edited %s: replaced '%s' with '%s'", path, old_str, new_str);
    return ESP_OK;
}

/**
 * @brief 列出目录
 */
esp_err_t tool_list_dir_execute(const char *params_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 解析参数 */
    cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
    if (!params) {
        snprintf(output, output_size, "错误: 参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *prefix = json_get_string(params, "prefix");
    if (!prefix || prefix[0] == '\0') {
        prefix = LITEARM_SPIFFS_BASE;
    }
    
    cJSON_Delete(params);
    
    /* 验证路径 */
    if (!is_valid_path(prefix)) {
        snprintf(output, output_size, "错误: 无效路径");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 打开目录 */
    DIR *dir = opendir(prefix);
    if (!dir) {
        snprintf(output, output_size, "错误: 目录不存在: %s", prefix);
        return ESP_ERR_NOT_FOUND;
    }
    
    /* 列出文件 */
    int len = snprintf(output, output_size, "目录: %s\n\n", prefix);
    int count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL && len < (int)output_size - 64) {
        /* 构建完整路径 */
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", prefix, entry->d_name);
        
        /* 获取文件信息 */
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (entry->d_type == DT_DIR) {
                len += snprintf(output + len, output_size - len, "📁 %s/\n", entry->d_name);
            } else {
                len += snprintf(output + len, output_size - len, "📄 %s (%d bytes)\n",
                               entry->d_name, (int)st.st_size);
            }
        } else {
            len += snprintf(output + len, output_size - len, "❓ %s\n", entry->d_name);
        }
        count++;
    }
    
    closedir(dir);
    
    len += snprintf(output + len, output_size - len, "\n共 %d 项", count);
    
    ESP_LOGI(TAG, "Listed %d items in %s", count, prefix);
    return ESP_OK;
}
