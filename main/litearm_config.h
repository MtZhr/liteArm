#pragma once

/* liteArm Configuration */

#include <stdint.h>

/* ============================================================================
 * Build-time secrets
 * ============================================================================ */
#include "litearm_secrets.h"

#ifndef LITEARM_SECRET_WIFI_SSID
#define LITEARM_SECRET_WIFI_SSID       ""
#endif
#ifndef LITEARM_SECRET_WIFI_PASS
#define LITEARM_SECRET_WIFI_PASS       ""
#endif
#ifndef LITEARM_SECRET_FEISHU_APP_ID
#define LITEARM_SECRET_FEISHU_APP_ID   ""
#endif
#ifndef LITEARM_SECRET_FEISHU_APP_SECRET
#define LITEARM_SECRET_FEISHU_APP_SECRET ""
#endif
#ifndef LITEARM_SECRET_PROXY_HOST
#define LITEARM_SECRET_PROXY_HOST      ""
#endif
#ifndef LITEARM_SECRET_PROXY_PORT
#define LITEARM_SECRET_PROXY_PORT      ""
#endif

/* ============================================================================
 * WiFi Configuration
 * ============================================================================ */
#define LITEARM_WIFI_MAX_RETRY          10
#define LITEARM_WIFI_RETRY_BASE_MS      1000
#define LITEARM_WIFI_RETRY_MAX_MS       30000

/* 初始 WiFi 凭证 (可通过物理按钮恢复) */
#ifndef LITEARM_INITIAL_WIFI_SSID
#define LITEARM_INITIAL_WIFI_SSID       ""
#endif
#ifndef LITEARM_INITIAL_WIFI_PASS
#define LITEARM_INITIAL_WIFI_PASS       ""
#endif

/* WiFi 重置按钮配置 */
#define LITEARM_WIFI_RESET_GPIO         0       /* 默认 GPIO0 (BOOT 按钮) */
#define LITEARM_WIFI_RESET_ACTIVE_LOW   true    /* 低电平触发 */
#define LITEARM_WIFI_RESET_HOLD_MS      5000    /* 长按 5 秒 */

/* WiFi 状态 LED 配置 */
#define LITEARM_WIFI_LED_GPIO           -1      /* 默认禁用, 设置 GPIO 启用 */
#define LITEARM_WIFI_LED_ACTIVE_LOW     true    /* 低电平点亮 */

/* ============================================================================
 * Feishu Bot Configuration
 * ============================================================================ */
#define LITEARM_FEISHU_MAX_MSG_LEN      4096
#define LITEARM_FEISHU_POLL_STACK       (16 * 1024)
#define LITEARM_FEISHU_POLL_PRIO        5
#define LITEARM_FEISHU_POLL_CORE        0
#define LITEARM_FEISHU_WEBHOOK_PORT     18790
#define LITEARM_FEISHU_WEBHOOK_PATH     "/feishu/events"
#define LITEARM_FEISHU_WEBHOOK_MAX_BODY (8 * 1024)

/* ============================================================================
 * Command Parser Configuration
 * ============================================================================ */
#define LITEARM_COMMAND_MAX_LEN          4096
#define LITEARM_TOOL_NAME_MAX_LEN        64
#define LITEARM_TOOL_PARAMS_MAX_LEN      2048

/* ============================================================================
 * Tool Executor Configuration
 * ============================================================================ */
#define LITEARM_TOOL_OUTPUT_SIZE         (8 * 1024)
#define LITEARM_MAX_TOOLS                16

/* ============================================================================
 * Message Bus Configuration
 * ============================================================================ */
#define LITEARM_BUS_QUEUE_LEN            8
#define LITEARM_OUTBOUND_STACK           (8 * 1024)
#define LITEARM_OUTBOUND_PRIO            5
#define LITEARM_OUTBOUND_CORE            0

/* ============================================================================
 * Memory / SPIFFS Configuration
 * ============================================================================ */
#define LITEARM_SPIFFS_BASE             "/spiffs"
#define LITEARM_SPIFFS_CONFIG_DIR       LITEARM_SPIFFS_BASE "/config"
#define LITEARM_SPIFFS_MEMORY_DIR       LITEARM_SPIFFS_BASE "/memory"
#define LITEARM_SPIFFS_SESSION_DIR      LITEARM_SPIFFS_BASE "/sessions"

/* ============================================================================
 * Serial CLI Configuration
 * ============================================================================ */
#define LITEARM_CLI_STACK               (4 * 1024)
#define LITEARM_CLI_PRIO                3
#define LITEARM_CLI_CORE                0

/* ============================================================================
 * Heartbeat Configuration
 * ============================================================================ */
#define LITEARM_HEARTBEAT_DEFAULT_INTERVAL_MS  (30 * 60 * 1000)   /* 30 minutes */
#define LITEARM_HEARTBEAT_MIN_INTERVAL_MS      (60 * 1000)        /* 1 minute */
#define LITEARM_HEARTBEAT_STACK                (4 * 1024)
#define LITEARM_HEARTBEAT_PRIO                 3

/* ============================================================================
 * NVS Configuration
 * ============================================================================ */
#define LITEARM_NVS_WIFI                "wifi_config"
#define LITEARM_NVS_FEISHU              "feishu_config"
#define LITEARM_NVS_TOOLS               "tools_config"

#define LITEARM_NVS_KEY_SSID            "ssid"
#define LITEARM_NVS_KEY_PASS            "password"
#define LITEARM_NVS_KEY_FEISHU_APP_ID   "app_id"
#define LITEARM_NVS_KEY_FEISHU_APP_SECRET "app_secret"

/* ============================================================================
 * Channel Names
 * ============================================================================ */
#define LITEARM_CHAN_FEISHU             "feishu"
#define LITEARM_CHAN_SYSTEM             "system"
#define LITEARM_CHAN_WEBSOCKET          "websocket"

/* ============================================================================
 * WiFi Onboarding
 * ============================================================================ */
#define LITEARM_ONBOARD_AP_PREFIX    "liteArm-"
#define LITEARM_ONBOARD_AP_PASS     ""
#define LITEARM_ONBOARD_HTTP_PORT   80
#define LITEARM_ONBOARD_DNS_STACK   (4 * 1024)
#define LITEARM_ONBOARD_MAX_SCAN    20
