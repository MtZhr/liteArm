/**
 * @file message_bus.h
 * @brief Message Bus - 消息队列模块
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "litearm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 消息结构 */
typedef struct {
    char channel[32];       /* 消息来源渠道 (feishu/system) */
    char chat_id[64];       /* 会话 ID */
    char sender_id[64];     /* 发送者 ID */
    char message_id[64];    /* 消息 ID (用于去重) */
    char *content;          /* 消息内容 (动态分配) */
    int msg_type;           /* 消息类型 */
} litearm_msg_t;

/**
 * @brief 初始化消息总线
 * @return ESP_OK 成功
 */
esp_err_t message_bus_init(void);

/**
 * @brief 推送消息到入站队列
 * @param msg 消息指针 (content 会被转移所有权)
 * @return ESP_OK 成功, ESP_ERR_TIMEOUT 队列满
 */
esp_err_t message_bus_push_inbound(litearm_msg_t *msg);

/**
 * @brief 从入站队列取出消息
 * @param msg 消息结构指针
 * @param timeout_ms 超时时间 (毫秒)
 * @return ESP_OK 成功, ESP_ERR_TIMEOUT 超时
 */
esp_err_t message_bus_pop_inbound(litearm_msg_t *msg, uint32_t timeout_ms);

/**
 * @brief 推送消息到出站队列
 * @param msg 消息指针 (content 会被转移所有权)
 * @return ESP_OK 成功, ESP_ERR_TIMEOUT 队列满
 */
esp_err_t message_bus_push_outbound(litearm_msg_t *msg);

/**
 * @brief 从出站队列取出消息
 * @param msg 消息结构指针
 * @param timeout_ms 超时时间 (毫秒)
 * @return ESP_OK 成功, ESP_ERR_TIMEOUT 超时
 */
esp_err_t message_bus_pop_outbound(litearm_msg_t *msg, uint32_t timeout_ms);

/**
 * @brief 获取入站队列待处理消息数
 * @return 消息数量
 */
int message_bus_inbound_count(void);

/**
 * @brief 获取出站队列待发送消息数
 * @return 消息数量
 */
int message_bus_outbound_count(void);

#ifdef __cplusplus
}
#endif
