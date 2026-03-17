/**
 * @file tool_system.c
 * @brief System Tools Implementation - 系统工具实现
 * 
 * 包含:
 * - set_wifi: 通过飞书配置 WiFi
 * - get_status: 获取系统状态
 * - set_heartbeat: 设置心跳目标
 * - reboot: 重启设备
 * - wifi_scan: 扫描 WiFi 网络
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "tool_system.h"
#include "litearm_config.h"
#include "wifi/wifi_manager.h"
#include "heartbeat/heartbeat.h"
#include "bus/message_bus.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "cJSON.h"

static const char *TAG = "sys_tools";

/**
 * @brief 设置 WiFi 凭证
 * 
 * 参数:
 * - ssid: WiFi 名称 (必需)
 * - password: WiFi 密码 (必需)
 * 
 * 示例:
 * !set_wifi[{"ssid": "MyWiFi", "password": "MyPassword"}]
 */
esp_err_t tool_set_wifi_execute(const char *params_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 解析参数 */
    cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
    if (!params) {
        snprintf(output, output_size, "❌ 参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 获取 SSID */
    cJSON *ssid_json = cJSON_GetObjectItem(params, "ssid");
    if (!ssid_json || !cJSON_IsString(ssid_json)) {
        cJSON_Delete(params);
        snprintf(output, output_size, "❌ 缺少必需参数: ssid");
        return ESP_ERR_INVALID_ARG;
    }
    const char *ssid = ssid_json->valuestring;
    
    /* 获取密码 */
    cJSON *pass_json = cJSON_GetObjectItem(params, "password");
    if (!pass_json || !cJSON_IsString(pass_json)) {
        cJSON_Delete(params);
        snprintf(output, output_size, "❌ 缺少必需参数: password");
        return ESP_ERR_INVALID_ARG;
    }
    const char *password = pass_json->valuestring;
    
    /* 验证参数 */
    if (strlen(ssid) == 0 || strlen(ssid) > 32) {
        cJSON_Delete(params);
        snprintf(output, output_size, "❌ SSID 长度无效 (1-32字符)");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(password) > 64) {
        cJSON_Delete(params);
        snprintf(output, output_size, "❌ 密码长度无效 (最多64字符)");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 保存 WiFi 凭证 */
    esp_err_t err = wifi_manager_set_credentials(ssid, password);
    
    cJSON_Delete(params);
    
    if (err == ESP_OK) {
        snprintf(output, output_size,
            "✅ WiFi 凭证已保存\n\n"
            "SSID: %s\n"
            "密码: %s\n\n"
            "⚠️ 需要重启设备才能连接新网络\n"
            "发送 !reboot 重启设备",
            ssid,
            strlen(password) > 8 ? "********" : password);
        ESP_LOGI(TAG, "WiFi credentials set: %s", ssid);
    } else {
        snprintf(output, output_size, "❌ 保存失败: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 获取系统状态
 * 
 * 参数: 无
 * 
 * 示例: !get_status
 */
esp_err_t tool_get_status_execute(const char *params_json, char *output, size_t output_size)
{
    (void)params_json;
    
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 获取系统信息 */
    uint32_t free_heap = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t min_heap = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    uint32_t largest_block = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    
    bool wifi_connected = wifi_manager_is_connected();
    const char *ip = wifi_manager_get_ip();
    const char *ssid = wifi_manager_get_ssid();
    
    bool heartbeat_running = heartbeat_is_running();
    
    /* 构建状态报告 */
    int len = snprintf(output, output_size,
        "📊 liteArm 系统状态\n\n"
        "📈 内存:\n"
        "  空闲: %lu bytes\n"
        "  最小: %lu bytes\n"
        "  最大块: %lu bytes\n\n",
        (unsigned long)free_heap,
        (unsigned long)min_heap,
        (unsigned long)largest_block);
    
    if (len < 0 || len >= (int)output_size - 200) {
        return ESP_OK;
    }
    
    len += snprintf(output + len, output_size - len,
        "📶 网络:\n"
        "  状态: %s\n"
        "  SSID: %s\n"
        "  IP: %s\n\n",
        wifi_connected ? "✅ 已连接" : "❌ 未连接",
        ssid[0] ? ssid : "-",
        ip);
    
    if (len < 0 || len >= (int)output_size - 100) {
        return ESP_OK;
    }
    
    len += snprintf(output + len, output_size - len,
        "💓 心跳: %s\n"
        "📥 入站队列: %d\n"
        "📤 出站队列: %d\n",
        heartbeat_running ? "运行中" : "已停止",
        message_bus_inbound_count(),
        message_bus_outbound_count());
    
    return ESP_OK;
}

/**
 * @brief 设置心跳目标
 * 
 * 参数:
 * - channel: 目标渠道 (如 "feishu")
 * - chat_id: 目标会话 ID
 * - interval_minutes: 心跳间隔 (分钟, 可选, 默认30)
 * 
 * 示例:
 * !set_heartbeat[{"channel": "feishu", "chat_id": "ou_xxxxx", "interval_minutes": 60}]
 */
esp_err_t tool_set_heartbeat_execute(const char *params_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 解析参数 */
    cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
    if (!params) {
        snprintf(output, output_size, "❌ 参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 获取渠道 */
    cJSON *channel_json = cJSON_GetObjectItem(params, "channel");
    if (!channel_json || !cJSON_IsString(channel_json)) {
        cJSON_Delete(params);
        snprintf(output, output_size, "❌ 缺少必需参数: channel\n示例: {\"channel\": \"feishu\", \"chat_id\": \"ou_xxx\"}");
        return ESP_ERR_INVALID_ARG;
    }
    const char *channel = channel_json->valuestring;
    
    /* 获取 chat_id */
    cJSON *chat_id_json = cJSON_GetObjectItem(params, "chat_id");
    if (!chat_id_json || !cJSON_IsString(chat_id_json)) {
        cJSON_Delete(params);
        snprintf(output, output_size, "❌ 缺少必需参数: chat_id\n示例: {\"channel\": \"feishu\", \"chat_id\": \"ou_xxx\"}");
        return ESP_ERR_INVALID_ARG;
    }
    const char *chat_id = chat_id_json->valuestring;
    
    /* 获取可选的间隔参数 */
    uint32_t interval_ms = 30 * 60 * 1000;  /* 默认 30 分钟 */
    cJSON *interval_json = cJSON_GetObjectItem(params, "interval_minutes");
    if (interval_json && cJSON_IsNumber(interval_json)) {
        int minutes = interval_json->valueint;
        if (minutes >= 1) {
            interval_ms = (uint32_t)minutes * 60 * 1000;
        }
    }
    
    /* 设置心跳 */
    esp_err_t err = heartbeat_set_target(channel, chat_id);
    if (err == ESP_OK) {
        heartbeat_set_interval(interval_ms);
        heartbeat_set_enabled(true);
        heartbeat_start();
        
        snprintf(output, output_size,
            "✅ 心跳已配置\n\n"
            "渠道: %s\n"
            "会话: %s\n"
            "间隔: %lu 分钟\n\n"
            "心跳将定期发送系统状态报告",
            channel,
            chat_id,
            (unsigned long)(interval_ms / 60000));
        
        ESP_LOGI(TAG, "Heartbeat set: %s:%s, interval=%lumin", 
                 channel, chat_id, (unsigned long)(interval_ms / 60000));
    } else {
        snprintf(output, output_size, "❌ 设置失败: %s", esp_err_to_name(err));
    }
    
    cJSON_Delete(params);
    return err;
}

/**
 * @brief 重启设备
 * 
 * 参数:
 * - confirm: 确认重启 (必需, 值为 "yes")
 * 
 * 示例: !reboot[{"confirm": "yes"}]
 */
esp_err_t tool_reboot_execute(const char *params_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 解析参数 */
    cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
    if (!params) {
        snprintf(output, output_size, "❌ 参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 获取确认参数 */
    cJSON *confirm_json = cJSON_GetObjectItem(params, "confirm");
    if (!confirm_json || !cJSON_IsString(confirm_json) || 
        strcmp(confirm_json->valuestring, "yes") != 0) {
        cJSON_Delete(params);
        snprintf(output, output_size, 
            "⚠️ 重启需要确认\n\n"
            "发送以下命令重启设备:\n"
            "!reboot[{\"confirm\": \"yes\"}]");
        return ESP_OK;
    }
    
    cJSON_Delete(params);
    
    snprintf(output, output_size, "🔄 设备正在重启...");
    
    /* 延迟重启, 确保消息发送 */
    ESP_LOGI(TAG, "Reboot requested via Feishu");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    
    return ESP_OK;
}

/**
 * @brief 扫描 WiFi 网络
 * 
 * 参数: 无
 * 
 * 示例: !wifi_scan
 */
esp_err_t tool_wifi_scan_execute(const char *params_json, char *output, size_t output_size)
{
    (void)params_json;
    
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 启动扫描 */
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };
    
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        snprintf(output, output_size, "❌ 扫描失败: %s", esp_err_to_name(err));
        return err;
    }
    
    /* 获取扫描结果 */
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        snprintf(output, output_size, "📡 未发现 WiFi 网络");
        return ESP_OK;
    }
    
    /* 限制最大数量 */
    if (ap_count > 20) ap_count = 20;
    
    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) {
        snprintf(output, output_size, "❌ 内存不足");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));
    
    /* 构建输出 */
    int len = snprintf(output, output_size, "📡 发现 %d 个 WiFi 网络:\n\n", ap_count);
    
    for (int i = 0; i < ap_count && len < (int)output_size - 64; i++) {
        /* 信号强度图标 */
        const char *signal_icon;
        int rssi = ap_list[i].rssi;
        if (rssi >= -50) signal_icon = "📶📶📶";
        else if (rssi >= -60) signal_icon = "📶📶";
        else if (rssi >= -70) signal_icon = "📶";
        else signal_icon = "📉";
        
        /* 加密类型 */
        const char *auth_str = "";
        switch (ap_list[i].authmode) {
            case WIFI_AUTH_OPEN: auth_str = "开放"; break;
            case WIFI_AUTH_WEP: auth_str = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: auth_str = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: auth_str = "WPA2"; break;
            case WIFI_AUTH_WPA3_PSK: auth_str = "WPA3"; break;
            default: auth_str = "加密"; break;
        }
        
        len += snprintf(output + len, output_size - len,
            "%d. %s %s\n"
            "   信号: %ddBm | %s | CH%d\n\n",
            i + 1,
            ap_list[i].ssid[0] ? (char *)ap_list[i].ssid : "(隐藏)",
            signal_icon,
            rssi,
            auth_str,
            ap_list[i].primary);
    }
    
    free(ap_list);
    
    return ESP_OK;
}

/**
 * @brief 初始化系统工具 (注册到工具表)
 */
void tool_system_init(void)
{
    /* 这个函数在 tool_registry_init 中调用 */
    /* 工具注册在 tool_registry.c 中完成 */
    ESP_LOGI(TAG, "System tools initialized");
}
