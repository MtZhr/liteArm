/**
 * @file agent_core.h
 * @brief Agent Core - 核心处理模块
 */

#pragma once

#include "esp_err.h"
#include "bus/message_bus.h"
#include "litearm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Agent 核心
 * @return ESP_OK 成功
 */
esp_err_t agent_core_init(void);

/**
 * @brief 启动 Agent 核心任务
 * @return ESP_OK 成功
 */
esp_err_t agent_core_start(void);

/**
 * @brief 处理单条消息
 * @param msg 消息指针
 * @return ESP_OK 成功
 */
esp_err_t agent_process_message(litearm_msg_t *msg);

#ifdef __cplusplus
}
#endif
