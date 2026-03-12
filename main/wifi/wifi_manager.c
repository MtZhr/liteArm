/**
 * @file wifi_manager.c
 * @brief WiFi Manager Implementation
 */

#include "wifi_manager.h"
#include "litearm_config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "wifi";

/* WiFi 事件组 */
static EventGroupHandle_t s_wifi_event_group = NULL;

/* 事件位定义 */
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

/* 连接状态 */
static bool s_connected = false;
static char s_ip_str[16] = {0};
static char s_ssid[64] = {0};
static int s_retry_count = 0;

/**
 * @brief WiFi 事件处理器
 */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started, connecting...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
                ESP_LOGW(TAG, "WiFi disconnected (reason: %d)", event->reason);
                s_connected = false;
                s_ip_str[0] = '\0';

                if (s_retry_count < LITEARM_WIFI_MAX_RETRY) {
                    esp_wifi_connect();
                    s_retry_count++;
                    ESP_LOGI(TAG, "Retry %d/%d", s_retry_count, LITEARM_WIFI_MAX_RETRY);
                    vTaskDelay(pdMS_TO_TICKS(LITEARM_WIFI_RETRY_BASE_MS));
                } else {
                    ESP_LOGE(TAG, "WiFi connection failed after %d retries", LITEARM_WIFI_MAX_RETRY);
                    if (s_wifi_event_group) {
                        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    }
                }
                break;
            }

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected to AP");
                s_retry_count = 0;
                break;

            default:
                break;
        }
    } else if (base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Got IP: %s", s_ip_str);
            s_connected = true;
            if (s_wifi_event_group) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            }
        }
    }
}

/**
 * @brief 初始化 WiFi 管理器
 */
esp_err_t wifi_manager_init(void)
{
    /* 创建事件组 */
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    /* 初始化网络接口 */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    /* 初始化 WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 注册事件处理器 */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

/**
 * @brief 启动 WiFi 连接
 */
esp_err_t wifi_manager_start(void)
{
    /* 从 NVS 读取凭证 */
    char ssid[64] = LITEARM_SECRET_WIFI_SSID;
    char password[64] = LITEARM_SECRET_WIFI_PASS;

    nvs_handle_t nvs;
    if (nvs_open(LITEARM_NVS_WIFI, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(ssid);
        if (nvs_get_str(nvs, LITEARM_NVS_KEY_SSID, ssid, &len) == ESP_OK && ssid[0]) {
            ESP_LOGI(TAG, "Loaded SSID from NVS: %s", ssid);
        }
        len = sizeof(password);
        if (nvs_get_str(nvs, LITEARM_NVS_KEY_PASS, password, &len) != ESP_OK) {
            strcpy(password, LITEARM_SECRET_WIFI_PASS);
        }
        nvs_close(nvs);
    }

    if (ssid[0] == '\0') {
        ESP_LOGW(TAG, "No WiFi credentials configured");
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);

    /* 配置 WiFi */
    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi connecting to '%s'...", ssid);
    return ESP_OK;
}

/**
 * @brief 等待 WiFi 连接
 */
esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms)
{
    if (!s_wifi_event_group) {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        return ESP_FAIL;
    } else {
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * @brief 检查 WiFi 是否已连接
 */
bool wifi_manager_is_connected(void)
{
    return s_connected;
}

/**
 * @brief 获取 IP 地址字符串
 */
const char* wifi_manager_get_ip(void)
{
    return s_connected ? s_ip_str : "0.0.0.0";
}

/**
 * @brief 获取 WiFi SSID
 */
const char* wifi_manager_get_ssid(void)
{
    return s_ssid;
}

/**
 * @brief 扫描并打印附近 AP
 */
void wifi_manager_scan_and_print(void)
{
    ESP_LOGI(TAG, "Scanning nearby WiFi networks...");

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(err));
        return;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count == 0) {
        ESP_LOGI(TAG, "No AP found");
        return;
    }

    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) {
        ESP_LOGE(TAG, "Failed to allocate AP list");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));

    ESP_LOGI(TAG, "Found %d APs:", ap_count);
    for (int i = 0; i < ap_count && i < 20; i++) {
        ESP_LOGI(TAG, "  %d: %s (rssi=%d, ch=%d, auth=%d)",
                 i + 1,
                 (char *)ap_list[i].ssid,
                 ap_list[i].rssi,
                 ap_list[i].primary,
                 ap_list[i].authmode);
    }

    free(ap_list);
}

/**
 * @brief 设置 WiFi 凭证
 */
esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(LITEARM_NVS_WIFI, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    nvs_set_str(nvs, LITEARM_NVS_KEY_SSID, ssid);
    nvs_set_str(nvs, LITEARM_NVS_KEY_PASS, password);
    nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "WiFi credentials saved: %s", ssid);
    return ESP_OK;
}

/**
 * @brief 断开 WiFi 连接
 */
void wifi_manager_disconnect(void)
{
    esp_wifi_disconnect();
    s_connected = false;
    s_ip_str[0] = '\0';
    s_retry_count = 0;
}
