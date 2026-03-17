/**
 * @file message_bus.c
 * @brief Message Bus Implementation
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "message_bus.h"
#include "litearm_config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "bus";

/* 消息队列句柄 */
static QueueHandle_t s_inbound_queue = NULL;
static QueueHandle_t s_outbound_queue = NULL;

/**
 * @brief 初始化消息总线
 */
esp_err_t message_bus_init(void)
{
    /* 创建入站队列 */
    s_inbound_queue = xQueueCreate(LITEARM_BUS_QUEUE_LEN, sizeof(litearm_msg_t));
    if (!s_inbound_queue) {
        ESP_LOGE(TAG, "Failed to create inbound queue");
        return ESP_ERR_NO_MEM;
    }
    
    /* 创建出站队列 */
    s_outbound_queue = xQueueCreate(LITEARM_BUS_QUEUE_LEN, sizeof(litearm_msg_t));
    if (!s_outbound_queue) {
        ESP_LOGE(TAG, "Failed to create outbound queue");
        vQueueDelete(s_inbound_queue);
        s_inbound_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Message bus initialized (queue size: %d)", LITEARM_BUS_QUEUE_LEN);
    return ESP_OK;
}

/**
 * @brief 推送消息到入站队列
 */
esp_err_t message_bus_push_inbound(litearm_msg_t *msg)
{
    if (!s_inbound_queue || !msg) {
        return ESP_ERR_INVALID_STATE;
    }
    
    /* 发送到队列 (不等待，队列满则失败) */
    if (xQueueSend(s_inbound_queue, msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Inbound queue full, dropping message");
        return ESP_ERR_TIMEOUT;
    }
    
    ESP_LOGD(TAG, "Inbound message queued: %s:%s", msg->channel, msg->chat_id);
    return ESP_OK;
}

/**
 * @brief 从入站队列取出消息
 */
esp_err_t message_bus_pop_inbound(litearm_msg_t *msg, uint32_t timeout_ms)
{
    if (!s_inbound_queue || !msg) {
        return ESP_ERR_INVALID_STATE;
    }
    
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    
    if (xQueueReceive(s_inbound_queue, msg, ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

/**
 * @brief 推送消息到出站队列
 */
esp_err_t message_bus_push_outbound(litearm_msg_t *msg)
{
    if (!s_outbound_queue || !msg) {
        return ESP_ERR_INVALID_STATE;
    }
    
    /* 发送到队列 (不等待，队列满则失败) */
    if (xQueueSend(s_outbound_queue, msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Outbound queue full, dropping message");
        return ESP_ERR_TIMEOUT;
    }
    
    ESP_LOGD(TAG, "Outbound message queued: %s:%s", msg->channel, msg->chat_id);
    return ESP_OK;
}

/**
 * @brief 从出站队列取出消息
 */
esp_err_t message_bus_pop_outbound(litearm_msg_t *msg, uint32_t timeout_ms)
{
    if (!s_outbound_queue || !msg) {
        return ESP_ERR_INVALID_STATE;
    }
    
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    
    if (xQueueReceive(s_outbound_queue, msg, ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

/**
 * @brief 获取入站队列待处理消息数
 */
int message_bus_inbound_count(void)
{
    if (!s_inbound_queue) return 0;
    return (int)uxQueueMessagesWaiting(s_inbound_queue);
}

/**
 * @brief 获取出站队列待发送消息数
 */
int message_bus_outbound_count(void)
{
    if (!s_outbound_queue) return 0;
    return (int)uxQueueMessagesWaiting(s_outbound_queue);
}
