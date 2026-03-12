/**
 * @file tool_get_time.h
 * @brief Get Time Tool Header
 */

#pragma once

#include "esp_err.h"

/**
 * @brief 初始化时间工具
 * @return ESP_OK 成功, 其他值失败
 */
esp_err_t tool_get_time_init(void);
