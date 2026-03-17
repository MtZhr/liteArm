/**
 * @file tool_system.h
 * @brief System Tools Header - 系统工具
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#pragma once

#include "esp_err.h"

/**
 * @brief 初始化系统工具
 */
void tool_system_init(void);

/**
 * @brief 设置 WiFi 凭证工具
 */
esp_err_t tool_set_wifi_execute(const char *params_json, char *output, size_t output_size);

/**
 * @brief 获取系统状态工具
 */
esp_err_t tool_get_status_execute(const char *params_json, char *output, size_t output_size);

/**
 * @brief 设置心跳目标工具
 */
esp_err_t tool_set_heartbeat_execute(const char *params_json, char *output, size_t output_size);

/**
 * @brief 重启设备工具
 */
esp_err_t tool_reboot_execute(const char *params_json, char *output, size_t output_size);

/**
 * @brief 扫描 WiFi 工具
 */
esp_err_t tool_wifi_scan_execute(const char *params_json, char *output, size_t output_size);
