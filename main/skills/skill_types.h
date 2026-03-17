/**
 * @file skill_types.h
 * @brief Skills 系统核心类型定义
 * 
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 技能参数类型
 * ============================================================================ */

typedef enum {
    SKILL_PARAM_STRING,     /**< 字符串类型 */
    SKILL_PARAM_NUMBER,     /**< 数字类型 */
    SKILL_PARAM_BOOLEAN,    /**< 布尔类型 */
    SKILL_PARAM_OBJECT,     /**< 对象类型 */
    SKILL_PARAM_ARRAY       /**< 数组类型 */
} skill_param_type_t;

/* ============================================================================
 * 技能参数定义
 * ============================================================================ */

typedef struct {
    const char *name;               /**< 参数名 */
    skill_param_type_t type;        /**< 参数类型 */
    bool required;                  /**< 是否必需 */
    const char *default_value;      /**< 默认值 */
    const char *description;        /**< 参数描述 */
} skill_param_def_t;

/* ============================================================================
 * 技能执行结果
 * ============================================================================ */

typedef struct {
    bool success;                   /**< 执行成功 */
    char message[512];              /**< 结果消息 */
    cJSON *data;                    /**< 结构化数据（可选） */
} skill_result_t;

/* ============================================================================
 * 技能执行函数类型
 * ============================================================================ */

/**
 * @brief 技能执行函数
 * @param params 参数（JSON对象）
 * @param result 执行结果
 * @return ESP_OK 成功，其他值失败
 */
typedef esp_err_t (*skill_execute_fn)(
    const cJSON *params,
    skill_result_t *result
);

/* ============================================================================
 * 技能定义
 * ============================================================================ */

typedef struct {
    const char *name;               /**< 技能名称 */
    const char *description;        /**< 技能描述 */
    const char *category;           /**< 分类 */
    skill_param_def_t *params;      /**< 参数列表 */
    int param_count;                /**< 参数数量 */
    skill_execute_fn execute;       /**< 执行函数 */
    const char **aliases;           /**< 别名列表 */
    int alias_count;                /**< 别名数量 */
} skill_def_t;

/* ============================================================================
 * 结果初始化和清理
 * ============================================================================ */

/**
 * @brief 初始化技能结果
 */
static inline void skill_result_init(skill_result_t *result) {
    if (result) {
        result->success = false;
        result->message[0] = '\0';
        result->data = NULL;
    }
}

/**
 * @brief 清理技能结果
 */
static inline void skill_result_cleanup(skill_result_t *result) {
    if (result && result->data) {
        cJSON_Delete(result->data);
        result->data = NULL;
    }
}

#ifdef __cplusplus
}
#endif
