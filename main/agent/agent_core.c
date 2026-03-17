#include "litearm_text.h"
/**
 * @file agent_core.c
 * @brief Agent Core Implementation
 * 
 * 核心处理逻辑:
 * 1. 从入站队列接收消息
 * 2. 解析命令格式
 * 3. 执行对应工具
 * 4. 将响应推送到出站队列
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "agent_core.h"
#include "command_parser.h"
#include "skills/skill_registry.h"
#include "bus/message_bus.h"
#include "channels/feishu/feishu_bot.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "agent";

/* 工具输出缓冲区大小 */
#define TOOL_OUTPUT_SIZE  LITEARM_TOOL_OUTPUT_SIZE

/* Agent 任务栈大小 */
#define AGENT_TASK_STACK  (8 * 1024)

/* Agent 任务优先级 */
#define AGENT_TASK_PRIO   6

/* Agent 任务核心 */
#define AGENT_TASK_CORE   1

/* 出站分发任务 */
static TaskHandle_t s_outbound_task = NULL;
static TaskHandle_t s_agent_task = NULL;

/**
 * @brief 构建错误响应
 */
static char *build_error_response(const char *error_msg)
{
    if (!error_msg) return NULL;
    
    size_t len = strlen(error_msg) + 32;
    char *resp = malloc(len);
    if (resp) {
        snprintf(resp, len, "❌ %s", error_msg);
    }
    return resp;
}

/**
 * @brief 构建成功响应
 */
static char *build_success_response(const char *skill_name, const char *result)
{
    if (!result) return NULL;
    
    size_t len = strlen(result) + 64;
    char *resp = malloc(len);
    if (resp) {
        snprintf(resp, len, "%s", result);
    }
    return resp;
}

/**
 * @brief 处理单条消息
 */
esp_err_t agent_process_message(litearm_msg_t *msg)
{
    if (!msg || !msg->content) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Processing message from %s:%s", msg->channel, msg->chat_id);
    
    /* 解析命令 */
    parse_result_t result;
    esp_err_t err = parse_command_text(msg->content, &result);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse message");
        parse_result_free(&result);
        return err;
    }
    
    char *response = NULL;
    
    if (result.is_skill_call && result.skill.is_valid) {
        /* 执行技能 */
        const skill_def_t *skill = skill_find(result.skill.skill_name);
        if (!skill) {
            response = build_error_response("未找到技能");
        } else {
            cJSON *params = cJSON_Parse(result.skill.params_json ? result.skill.params_json : "{}");
            skill_result_t skill_result;
            skill_result_init(&skill_result);
            
            esp_err_t exec_err = skill->execute(params, &skill_result);
            cJSON_Delete(params);
            
            if (exec_err == ESP_OK && skill_result.success) {
                response = build_success_response(result.skill.skill_name, skill_result.message);
            } else {
                response = build_error_response(skill_result.message);
            }
            
            skill_result_cleanup(&skill_result);
        }
    } else {
        /* 不是工具调用，返回帮助信息 */
        response = strdup(
            TXT_MSG_WELCOME
            "发送 !help 查看可用命令\n"
            "格式: !工具名[参数]\n\n"
            "示例:\n"
            "  !get_time\n"
            "  !read_file[{\"path\": \"/spiffs/config.txt\"}]"
        );
    }
    
    parse_result_free(&result);
    
    /* 发送响应 */
    if (response) {
        litearm_msg_t out = {0};
        strncpy(out.channel, msg->channel, sizeof(out.channel) - 1);
        strncpy(out.chat_id, msg->chat_id, sizeof(out.chat_id) - 1);
        out.content = response;
        
        if (message_bus_push_outbound(&out) != ESP_OK) {
            ESP_LOGW(TAG, "Outbound queue full, dropping response");
            free(response);
        } else {
            ESP_LOGI(TAG, "Response queued for %s:%s", out.channel, out.chat_id);
        }
    }
    
    return ESP_OK;
}

/**
 * @brief Agent 主循环任务
 */
static void agent_loop_task(void *arg)
{
    (void)arg;
    
    ESP_LOGI(TAG, "Agent loop started on core %d", xPortGetCoreID());
    
    while (1) {
        litearm_msg_t msg;
        esp_err_t err = message_bus_pop_inbound(&msg, UINT32_MAX);
        
        if (err == ESP_OK) {
            agent_process_message(&msg);
            
            /* 释放消息内容 */
            if (msg.content) {
                free(msg.content);
            }
        }
        
        /* 记录内存状态 */
        ESP_LOGD(TAG, "Free heap: %d bytes", 
                 (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
}

/**
 * @brief 出站分发任务
 */
static void outbound_dispatch_task(void *arg)
{
    (void)arg;
    
    ESP_LOGI(TAG, "Outbound dispatch started");
    
    while (1) {
        litearm_msg_t msg;
        if (message_bus_pop_outbound(&msg, UINT32_MAX) != ESP_OK) continue;
        
        ESP_LOGI(TAG, "Dispatching response to %s:%s", msg.channel, msg.chat_id);
        
        /* 根据渠道发送 */
        if (strcmp(msg.channel, LITEARM_CHAN_FEISHU) == 0) {
            esp_err_t send_err = feishu_send_message(msg.chat_id, msg.content);
            if (send_err != ESP_OK) {
                ESP_LOGE(TAG, "Feishu send failed: %s", esp_err_to_name(send_err));
            } else {
                ESP_LOGI(TAG, "Feishu send success for %s", msg.chat_id);
            }
        } else if (strcmp(msg.channel, LITEARM_CHAN_SYSTEM) == 0) {
            ESP_LOGI(TAG, "System message: %.128s", msg.content);
        } else {
            ESP_LOGW(TAG, "Unknown channel: %s", msg.channel);
        }
        
        /* 释放内容 */
        free(msg.content);
    }
}

/* 用于 Cron 任务推送消息的接口 */
esp_err_t message_bus_push_inbound_from_cron(litearm_msg_t *msg)
{
    return message_bus_push_inbound(msg);
}

/**
 * @brief 初始化 Agent 核心
 */
esp_err_t agent_core_init(void)
{
    ESP_LOGI(TAG, "Agent core initialized");
    return ESP_OK;
}

/**
 * @brief 启动 Agent 核心
 */
esp_err_t agent_core_start(void)
{
    /* 创建出站分发任务 */
    BaseType_t ret = xTaskCreatePinnedToCore(
        outbound_dispatch_task,
        "outbound",
        LITEARM_OUTBOUND_STACK,
        NULL,
        LITEARM_OUTBOUND_PRIO,
        &s_outbound_task,
        LITEARM_OUTBOUND_CORE);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create outbound task");
        return ESP_FAIL;
    }
    
    /* 创建 Agent 循环任务 */
    ret = xTaskCreatePinnedToCore(
        agent_loop_task,
        "agent_loop",
        AGENT_TASK_STACK,
        NULL,
        AGENT_TASK_PRIO,
        &s_agent_task,
        AGENT_TASK_CORE);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create agent task");
        vTaskDelete(s_outbound_task);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Agent core started");
    return ESP_OK;
}
