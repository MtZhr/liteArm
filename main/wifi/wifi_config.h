/**
 * @file wifi_config.h
 * @brief WiFi Configuration - WiFi 配网模块
 * 
 * 功能:
 * - 支持初始 WiFi 凭证配置
 * - 物理按钮重置到初始密码
 * - 配网状态 LED 指示
 * - 配网超时处理
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 配网状态 */
typedef enum {
    WIFI_CONFIG_STATE_IDLE,         /* 空闲 */
    WIFI_CONFIG_STATE_CONNECTING,   /* 连接中 */
    WIFI_CONFIG_STATE_CONNECTED,    /* 已连接 */
    WIFI_CONFIG_STATE_FAILED,       /* 连接失败 */
    WIFI_CONFIG_STATE_RESET,        /* 重置中 */
} wifi_config_state_t;

/**
 * @brief 初始化 WiFi 配网模块
 * @return ESP_OK 成功
 */
esp_err_t wifi_config_init(void);

/**
 * @brief 设置初始 WiFi 凭证
 * @param ssid WiFi 名称
 * @param password WiFi 密码
 * @return ESP_OK 成功
 */
esp_err_t wifi_config_set_initial(const char *ssid, const char *password);

/**
 * @brief 获取初始 WiFi 凭证
 * @param ssid 输出 SSID 缓冲区
 * @param password 输出密码缓冲区
 * @param ssid_size SSID 缓冲区大小
 * @param pass_size 密码缓冲区大小
 * @return ESP_OK 成功
 */
esp_err_t wifi_config_get_initial(char *ssid, char *password, size_t ssid_size, size_t pass_size);

/**
 * @brief 恢复到初始 WiFi 配置
 * @return ESP_OK 成功
 */
esp_err_t wifi_config_reset_to_initial(void);

/**
 * @brief 检查当前配置是否为初始配置
 * @return true 是初始配置
 */
bool wifi_config_is_initial(void);

/**
 * @brief 获取配网状态
 * @return 配网状态
 */
wifi_config_state_t wifi_config_get_state(void);

/**
 * @brief 设置重置按钮 GPIO
 * @param gpio_num GPIO 编号
 * @param active_low true: 低电平触发, false: 高电平触发
 * @param hold_ms 长按时间 (毫秒)
 * @return ESP_OK 成功
 */
esp_err_t wifi_config_set_reset_button(int gpio_num, bool active_low, uint32_t hold_ms);

/**
 * @brief 手动触发重置 (用于测试或软件触发)
 * @return ESP_OK 成功
 */
esp_err_t wifi_config_trigger_reset(void);

/**
 * @brief 启用 LED 状态指示
 * @param gpio_num LED GPIO 编号
 * @param active_low true: 低电平点亮
 * @return ESP_OK 成功
 */
esp_err_t wifi_config_enable_status_led(int gpio_num, bool active_low);

/**
 * @brief 禁用 LED 状态指示
 */
void wifi_config_disable_status_led(void);

#ifdef __cplusplus
}
#endif
