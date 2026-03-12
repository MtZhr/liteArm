/**
 * @file feishu_bot.h
 * @brief Feishu Bot Module - 飞书消息接收与发送模块
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "litearm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化飞书 Bot
 * @return ESP_OK 成功
 */
esp_err_t feishu_bot_init(void);

/**
 * @brief 启动飞书 Bot (WebSocket 连接)
 * @return ESP_OK 成功
 */
esp_err_t feishu_bot_start(void);

/**
 * @brief 发送文本消息
 * @param chat_id 目标会话 ID (可以是 open_id 或 chat_id)
 * @param text 文本内容
 * @return ESP_OK 成功
 */
esp_err_t feishu_send_message(const char *chat_id, const char *text);

/**
 * @brief 回复消息
 * @param message_id 原消息 ID
 * @param text 回复内容
 * @return ESP_OK 成功
 */
esp_err_t feishu_reply_message(const char *message_id, const char *text);

/**
 * @brief 设置飞书凭证
 * @param app_id 应用 ID
 * @param app_secret 应用密钥
 * @return ESP_OK 成功
 */
esp_err_t feishu_set_credentials(const char *app_id, const char *app_secret);

/**
 * @brief 检查飞书是否已配置
 * @return true 已配置
 */
bool feishu_is_configured(void);

#ifdef __cplusplus
}
#endif
