/**
 * @file heartbeat.h
 * @brief Heartbeat - 心跳监控模块
 * 
 * 定期发送心跳消息，用于:
 * - 监控系统运行状态
 * - 定期健康检查
 * - 可配置的心跳间隔和目标渠道
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

/**
 * @brief 初始化心跳模块
 * @return ESP_OK 成功
 */
esp_err_t heartbeat_init(void);

/**
 * @brief 启动心跳任务
 * @return ESP_OK 成功
 */
esp_err_t heartbeat_start(void);

/**
 * @brief 停止心跳任务
 */
void heartbeat_stop(void);

/**
 * @brief 设置心跳间隔
 * @param interval_ms 间隔时间 (毫秒)
 */
void heartbeat_set_interval(uint32_t interval_ms);

/**
 * @brief 设置心跳目标
 * @param channel 目标渠道 (如 "feishu")
 * @param chat_id 目标会话 ID
 * @return ESP_OK 成功
 */
esp_err_t heartbeat_set_target(const char *channel, const char *chat_id);

/**
 * @brief 启用/禁用心跳
 * @param enabled true 启用
 */
void heartbeat_set_enabled(bool enabled);

/**
 * @brief 检查心跳是否运行
 * @return true 运行中
 */
bool heartbeat_is_running(void);

/**
 * @brief 触发立即心跳
 */
void heartbeat_trigger_now(void);

#ifdef __cplusplus
}
#endif
