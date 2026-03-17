/**
 * @file skill_registry.h
 * @brief 技能注册表接口
 * 
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#pragma once

#include "skill_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化技能注册表
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t skill_registry_init(void);

/**
 * @brief 注册技能
 * @param skill 技能定义
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t skill_register(const skill_def_t *skill);

/**
 * @brief 查找技能
 * @param name 技能名称或别名
 * @return 技能定义指针，未找到返回NULL
 */
const skill_def_t* skill_find(const char *name);

/**
 * @brief 获取所有技能列表
 * @param count 返回技能数量
 * @return 技能数组指针
 */
const skill_def_t* skill_list(int *count);

/**
 * @brief 获取技能数量
 * @return 技能数量
 */
int skill_registry_count(void);

/**
 * @brief 清理技能注册表
 */
void skill_registry_cleanup(void);

#ifdef __cplusplus
}
#endif
