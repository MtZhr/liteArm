/**
 * @file command_parser.h
 * @brief Command Parser Module - 消息解析模块
 * 
 * 支持的消息格式:
 * - !tool_name
 * - !tool_name[{"param": "value"}]
 * - /tool_name param
 * - /tool_name[{"param": "value"}]
 * - help
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "litearm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 消息类型 */
typedef enum {
    MSG_TYPE_TEXT,        /* 文本消息 */
    MSG_TYPE_IMAGE,       /* 图片消息 */
    MSG_TYPE_VOICE,       /* 语音消息 */
    MSG_TYPE_FILE,       /* 文件消息 */
    MSG_TYPE_CARD,       /* 卡片消息 */
    MSG_TYPE_UNKNOWN     /* 未知类型 */
} msg_type_t;

/* 工具调用请求 */
typedef struct {
    char tool_name[LITEARM_TOOL_NAME_MAX_LEN];    /* 工具名称 */
    char *params_json;                          /* 参数 JSON 字符串 */
    bool is_valid;                              /* 是否为有效工具调用 */
} tool_request_t;

/* 飞书消息结构 */
typedef struct {
    msg_type_t type;
    char message_id[64];
    char chat_id[64];
    char sender_id[64];
    char chat_type[16];       /* p2p / group */
    union {
        char *text;           /* 文本内容 */
        char *image_key;      /* 图片 key */
        char *file_key;       /* 文件 key */
        char *voice_key;      /* 语音 key */
    } content;
} feishu_message_t;

/* 解析结果 */
typedef struct {
    bool is_tool_call;        /* 是否为工具调用 */
    tool_request_t tool;      /* 工具请求 */
    char *raw_text;          /* 原始文本 */
    msg_type_t msg_type;     /* 消息类型 */
} parse_result_t;

/**
 * @brief 解析飞书消息
 * @param msg 飞书消息结构
 * @param result 解析结果
 * @return ESP_OK 成功, 其他失败
 */
esp_err_t parse_feishu_message(const feishu_message_t *msg, parse_result_t *result);

/**
 * @brief 解析文本命令
 * @param text 输入文本
 * @param result 解析结果
 * @return ESP_OK 成功, 其他失败
 */
esp_err_t parse_command_text(const char *text, parse_result_t *result);

/**
 * @brief 释放解析结果资源
 * @param result 解析结果
 */
void parse_result_free(parse_result_t *result);

/**
 * @brief 获取帮助信息
 * @return 帮助文本字符串
 */
const char* command_parser_get_help_text(void);

#ifdef __cplusplus
}
#endif
