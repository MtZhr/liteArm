/**
 * @file skill_get_time.h
 * @brief 获取时间技能
 * 
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#pragma once

#include "../skill_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册获取时间技能
 * @return ESP_OK 成功
 */
esp_err_t skill_get_time_register(void);

#ifdef __cplusplus
}
#endif
