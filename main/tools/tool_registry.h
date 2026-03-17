/**
 * @file tool_registry.h
 * @brief Tool Registry - 工具注册与执行模块
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "litearm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 工具执行函数类型 */
typedef esp_err_t (*tool_execute_fn)(const char *params_json, char *output, size_t output_size);

/* 工具定义 */
typedef struct {
    const char *name;               /* 工具名称 */
    const char *description;        /* 工具描述 */
    const char *param_schema;       /* 参数 JSON Schema */
    tool_execute_fn execute;        /* 执行函数 */
} litearm_tool_t;

/**
 * @brief 初始化工具注册表
 * @return ESP_OK 成功
 */
esp_err_t tool_registry_init(void);

/**
 * @brief 注册工具
 * @param tool 工具定义
 * @return ESP_OK 成功, ESP_ERR_NO_MEM 注册表已满
 */
esp_err_t tool_registry_register(const litearm_tool_t *tool);

/**
 * @brief 执行工具
 * @param name 工具名称
 * @param params_json 参数 JSON 字符串
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return ESP_OK 成功, ESP_ERR_NOT_FOUND 工具不存在
 */
esp_err_t tool_registry_execute(const char *name, const char *params_json,
                                char *output, size_t output_size);

/**
 * @brief 检查工具是否存在
 * @param name 工具名称
 * @return true 存在, false 不存在
 */
bool tool_registry_exists(const char *name);

/**
 * @brief 获取工具 JSON 描述（用于帮助信息）
 * @return JSON 字符串（静态内存）
 */
const char* tool_registry_get_tools_json(void);

/**
 * @brief 获取工具数量
 * @return 工具数量
 */
int tool_registry_get_count(void);

#ifdef __cplusplus
}
#endif
