/**
 * @file wifi_manager.h
 * @brief WiFi Manager - WiFi 连接管理模块
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 WiFi 管理器
 * @return ESP_OK 成功
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief 启动 WiFi 连接
 * @return ESP_OK 成功
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief 等待 WiFi 连接
 * @param timeout_ms 超时时间 (毫秒)
 * @return ESP_OK 成功, ESP_ERR_TIMEOUT 超时
 */
esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms);

/**
 * @brief 检查 WiFi 是否已连接
 * @return true 已连接
 */
bool wifi_manager_is_connected(void);

/**
 * @brief 获取 IP 地址字符串
 * @return IP 地址字符串 (静态缓冲区)
 */
const char* wifi_manager_get_ip(void);

/**
 * @brief 获取 WiFi SSID
 * @return SSID 字符串 (静态缓冲区)
 */
const char* wifi_manager_get_ssid(void);

/**
 * @brief 扫描并打印附近 AP
 */
void wifi_manager_scan_and_print(void);

/**
 * @brief 设置 WiFi 凭证
 * @param ssid WiFi 名称
 * @param password WiFi 密码
 * @return ESP_OK 成功
 */
esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password);

/**
 * @brief 断开 WiFi 连接
 */
void wifi_manager_disconnect(void);

#ifdef __cplusplus
}
#endif
