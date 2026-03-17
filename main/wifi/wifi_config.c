/**
 * @file wifi_config.c
 * @brief WiFi Configuration Implementation
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "wifi_config.h"
#include "wifi_manager.h"
#include "litearm_config.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "nvs.h"

static const char *TAG = "wifi_cfg";

/* NVS 键名 */
#define NVS_NAMESPACE       "wifi_cfg"
#define NVS_KEY_INIT_SSID   "init_ssid"
#define NVS_KEY_INIT_PASS   "init_pass"
#define NVS_KEY_CURR_SSID   "curr_ssid"
#define NVS_KEY_CURR_PASS   "curr_pass"

/* 初始凭证 (编译时可配置) */
#ifndef LITEARM_INITIAL_WIFI_SSID
#define LITEARM_INITIAL_WIFI_SSID    ""
#endif
#ifndef LITEARM_INITIAL_WIFI_PASS
#define LITEARM_INITIAL_WIFI_PASS    ""
#endif

/* 按钮配置 */
#define DEFAULT_RESET_GPIO        -1
#define DEFAULT_RESET_ACTIVE_LOW  true
#define DEFAULT_RESET_HOLD_MS     5000  /* 5秒长按 */

/* LED 闪烁间隔 */
#define LED_BLINK_CONNECTING_MS   500
#define LED_BLINK_FAILED_MS       200
#define LED_BLINK_RESET_MS        100

/* 状态 */
static struct {
    char initial_ssid[64];
    char initial_pass[64];
    char current_ssid[64];
    char current_pass[64];
    
    wifi_config_state_t state;
    
    /* 重置按钮 */
    int reset_gpio;
    bool reset_active_low;
    uint32_t reset_hold_ms;
    bool reset_enabled;
    
    /* LED */
    int led_gpio;
    bool led_active_low;
    bool led_enabled;
    TimerHandle_t led_timer;
    
    /* 按钮检测 */
    int64_t button_press_time;
    bool button_pressed;
} s_cfg = {0};

/* 前向声明 */
static void led_timer_callback(TimerHandle_t timer);
static void button_check_task(void *arg);

/**
 * @brief 从 NVS 加载配置
 */
static esp_err_t load_config_from_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    
    if (err == ESP_OK) {
        size_t len;
        
        /* 加载初始凭证 */
        len = sizeof(s_cfg.initial_ssid);
        if (nvs_get_str(nvs, NVS_KEY_INIT_SSID, s_cfg.initial_ssid, &len) != ESP_OK) {
            strncpy(s_cfg.initial_ssid, LITEARM_INITIAL_WIFI_SSID, sizeof(s_cfg.initial_ssid) - 1);
        }
        
        len = sizeof(s_cfg.initial_pass);
        if (nvs_get_str(nvs, NVS_KEY_INIT_PASS, s_cfg.initial_pass, &len) != ESP_OK) {
            strncpy(s_cfg.initial_pass, LITEARM_INITIAL_WIFI_PASS, sizeof(s_cfg.initial_pass) - 1);
        }
        
        /* 加载当前凭证 */
        len = sizeof(s_cfg.current_ssid);
        if (nvs_get_str(nvs, NVS_KEY_CURR_SSID, s_cfg.current_ssid, &len) != ESP_OK) {
            /* 使用初始凭证作为当前凭证 */
            strncpy(s_cfg.current_ssid, s_cfg.initial_ssid, sizeof(s_cfg.current_ssid) - 1);
        }
        
        len = sizeof(s_cfg.current_pass);
        if (nvs_get_str(nvs, NVS_KEY_CURR_PASS, s_cfg.current_pass, &len) != ESP_OK) {
            strncpy(s_cfg.current_pass, s_cfg.initial_pass, sizeof(s_cfg.current_pass) - 1);
        }
        
        nvs_close(nvs);
    } else {
        /* NVS 不存在, 使用编译时默认值 */
        strncpy(s_cfg.initial_ssid, LITEARM_INITIAL_WIFI_SSID, sizeof(s_cfg.initial_ssid) - 1);
        strncpy(s_cfg.initial_pass, LITEARM_INITIAL_WIFI_PASS, sizeof(s_cfg.initial_pass) - 1);
        strncpy(s_cfg.current_ssid, LITEARM_INITIAL_WIFI_SSID, sizeof(s_cfg.current_ssid) - 1);
        strncpy(s_cfg.current_pass, LITEARM_INITIAL_WIFI_PASS, sizeof(s_cfg.current_pass) - 1);
    }
    
    ESP_LOGI(TAG, "Initial WiFi: %s", s_cfg.initial_ssid[0] ? s_cfg.initial_ssid : "(not set)");
    ESP_LOGI(TAG, "Current WiFi: %s", s_cfg.current_ssid[0] ? s_cfg.current_ssid : "(not set)");
    
    return ESP_OK;
}

/**
 * @brief 保存当前配置到 NVS
 */
static esp_err_t save_current_config(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    
    nvs_set_str(nvs, NVS_KEY_CURR_SSID, s_cfg.current_ssid);
    nvs_set_str(nvs, NVS_KEY_CURR_PASS, s_cfg.current_pass);
    nvs_commit(nvs);
    nvs_close(nvs);
    
    ESP_LOGI(TAG, "Saved current config: %s", s_cfg.current_ssid);
    return ESP_OK;
}

/**
 * @brief 保存初始配置到 NVS
 */
static esp_err_t save_initial_config(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    
    nvs_set_str(nvs, NVS_KEY_INIT_SSID, s_cfg.initial_ssid);
    nvs_set_str(nvs, NVS_KEY_INIT_PASS, s_cfg.initial_pass);
    nvs_commit(nvs);
    nvs_close(nvs);
    
    ESP_LOGI(TAG, "Saved initial config: %s", s_cfg.initial_ssid);
    return ESP_OK;
}

/**
 * @brief 设置 LED 状态
 */
static void set_led(bool on)
{
    if (!s_cfg.led_enabled || s_cfg.led_gpio < 0) return;
    
    int level = on ? (s_cfg.led_active_low ? 0 : 1) : (s_cfg.led_active_low ? 1 : 0);
    gpio_set_level(s_cfg.led_gpio, level);
}

/**
 * @brief LED 定时器回调
 */
static void led_timer_callback(TimerHandle_t timer)
{
    static bool led_state = false;
    
    switch (s_cfg.state) {
        case WIFI_CONFIG_STATE_CONNECTING:
            led_state = !led_state;
            set_led(led_state);
            break;
            
        case WIFI_CONFIG_STATE_FAILED:
            led_state = !led_state;
            set_led(led_state);
            break;
            
        case WIFI_CONFIG_STATE_RESET:
            led_state = !led_state;
            set_led(led_state);
            break;
            
        case WIFI_CONFIG_STATE_CONNECTED:
            set_led(true);
            break;
            
        default:
            set_led(false);
            break;
    }
    
    (void)timer;
}

/**
 * @brief 更新 LED 状态
 */
static void update_led_state(void)
{
    if (!s_cfg.led_enabled || s_cfg.led_gpio < 0) return;
    
    switch (s_cfg.state) {
        case WIFI_CONFIG_STATE_CONNECTING:
            xTimerChangePeriod(s_cfg.led_timer, pdMS_TO_TICKS(LED_BLINK_CONNECTING_MS), 0);
            xTimerStart(s_cfg.led_timer, 0);
            break;
            
        case WIFI_CONFIG_STATE_FAILED:
            xTimerChangePeriod(s_cfg.led_timer, pdMS_TO_TICKS(LED_BLINK_FAILED_MS), 0);
            xTimerStart(s_cfg.led_timer, 0);
            break;
            
        case WIFI_CONFIG_STATE_RESET:
            xTimerChangePeriod(s_cfg.led_timer, pdMS_TO_TICKS(LED_BLINK_RESET_MS), 0);
            xTimerStart(s_cfg.led_timer, 0);
            break;
            
        case WIFI_CONFIG_STATE_CONNECTED:
            xTimerStop(s_cfg.led_timer, 0);
            set_led(true);
            break;
            
        default:
            xTimerStop(s_cfg.led_timer, 0);
            set_led(false);
            break;
    }
}

/**
 * @brief 设置配网状态
 */
static void set_state(wifi_config_state_t state)
{
    if (s_cfg.state != state) {
        s_cfg.state = state;
        ESP_LOGI(TAG, "State changed to: %d", state);
        update_led_state();
    }
}

/**
 * @brief 按钮检测任务
 */
static void button_check_task(void *arg)
{
    (void)arg;
    
    ESP_LOGI(TAG, "Button check task started (GPIO=%d, hold=%lums)", 
             s_cfg.reset_gpio, (unsigned long)s_cfg.reset_hold_ms);
    
    while (s_cfg.reset_enabled) {
        if (s_cfg.reset_gpio >= 0) {
            int level = gpio_get_level(s_cfg.reset_gpio);
            bool pressed = s_cfg.reset_active_low ? (level == 0) : (level == 1);
            
            if (pressed && !s_cfg.button_pressed) {
                /* 按钮按下 */
                s_cfg.button_pressed = true;
                s_cfg.button_press_time = esp_timer_get_time();
                ESP_LOGI(TAG, "Button pressed");
                
            } else if (!pressed && s_cfg.button_pressed) {
                /* 按钮释放 */
                s_cfg.button_pressed = false;
                int64_t duration = (esp_timer_get_time() - s_cfg.button_press_time) / 1000;
                ESP_LOGI(TAG, "Button released (held %lld ms)", duration);
                
            } else if (pressed && s_cfg.button_pressed) {
                /* 检查长按时间 */
                int64_t duration = (esp_timer_get_time() - s_cfg.button_press_time) / 1000;
                
                if (duration >= s_cfg.reset_hold_ms) {
                    ESP_LOGW(TAG, "!!! RESET BUTTON TRIGGERED !!!");
                    
                    /* 触发重置 */
                    set_state(WIFI_CONFIG_STATE_RESET);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    
                    wifi_config_reset_to_initial();
                    
                    /* 重启 */
                    ESP_LOGI(TAG, "Rebooting...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "Button check task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief 初始化 WiFi 配网模块
 */
esp_err_t wifi_config_init(void)
{
    /* 加载配置 */
    load_config_from_nvs();
    
    /* 初始化状态 */
    s_cfg.state = WIFI_CONFIG_STATE_IDLE;
    s_cfg.reset_gpio = DEFAULT_RESET_GPIO;
    s_cfg.reset_active_low = DEFAULT_RESET_ACTIVE_LOW;
    s_cfg.reset_hold_ms = DEFAULT_RESET_HOLD_MS;
    s_cfg.reset_enabled = false;
    s_cfg.led_gpio = -1;
    s_cfg.led_enabled = false;
    s_cfg.button_pressed = false;
    
    /* 创建 LED 定时器 */
    s_cfg.led_timer = xTimerCreate("led_blink", pdMS_TO_TICKS(500), 
                                    pdTRUE, NULL, led_timer_callback);
    
    ESP_LOGI(TAG, "WiFi config initialized");
    return ESP_OK;
}

/**
 * @brief 设置初始 WiFi 凭证
 */
esp_err_t wifi_config_set_initial(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(s_cfg.initial_ssid, ssid, sizeof(s_cfg.initial_ssid) - 1);
    strncpy(s_cfg.initial_pass, password, sizeof(s_cfg.initial_pass) - 1);
    
    /* 如果当前没有配置, 同时设置为当前配置 */
    if (s_cfg.current_ssid[0] == '\0') {
        strncpy(s_cfg.current_ssid, ssid, sizeof(s_cfg.current_ssid) - 1);
        strncpy(s_cfg.current_pass, password, sizeof(s_cfg.current_pass) - 1);
        save_current_config();
    }
    
    return save_initial_config();
}

/**
 * @brief 获取初始 WiFi 凭证
 */
esp_err_t wifi_config_get_initial(char *ssid, char *password, size_t ssid_size, size_t pass_size)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(ssid, s_cfg.initial_ssid, ssid_size - 1);
    ssid[ssid_size - 1] = '\0';
    
    strncpy(password, s_cfg.initial_pass, pass_size - 1);
    password[pass_size - 1] = '\0';
    
    return ESP_OK;
}

/**
 * @brief 恢复到初始 WiFi 配置
 */
esp_err_t wifi_config_reset_to_initial(void)
{
    if (s_cfg.initial_ssid[0] == '\0') {
        ESP_LOGW(TAG, "No initial WiFi configured");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Resetting to initial WiFi: %s", s_cfg.initial_ssid);
    
    /* 复制初始凭证到当前凭证 */
    strncpy(s_cfg.current_ssid, s_cfg.initial_ssid, sizeof(s_cfg.current_ssid) - 1);
    strncpy(s_cfg.current_pass, s_cfg.initial_pass, sizeof(s_cfg.current_pass) - 1);
    
    /* 保存 */
    save_current_config();
    
    /* 更新 WiFi Manager */
    esp_err_t err = wifi_manager_set_credentials(s_cfg.initial_ssid, s_cfg.initial_pass);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi credentials: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "WiFi reset to initial configuration");
    return ESP_OK;
}

/**
 * @brief 检查当前配置是否为初始配置
 */
bool wifi_config_is_initial(void)
{
    return strcmp(s_cfg.current_ssid, s_cfg.initial_ssid) == 0 &&
           strcmp(s_cfg.current_pass, s_cfg.initial_pass) == 0;
}

/**
 * @brief 获取配网状态
 */
wifi_config_state_t wifi_config_get_state(void)
{
    return s_cfg.state;
}

/**
 * @brief 设置重置按钮 GPIO
 */
esp_err_t wifi_config_set_reset_button(int gpio_num, bool active_low, uint32_t hold_ms)
{
    s_cfg.reset_gpio = gpio_num;
    s_cfg.reset_active_low = active_low;
    s_cfg.reset_hold_ms = hold_ms;
    
    if (gpio_num >= 0) {
        /* 配置 GPIO */
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << gpio_num),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
            .pull_down_en = active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure button GPIO: %s", esp_err_to_name(err));
            return err;
        }
        
        /* 启动检测任务 */
        s_cfg.reset_enabled = true;
        
        BaseType_t ret = xTaskCreatePinnedToCore(
            button_check_task, "btn_check", 2048, NULL, 5, NULL, 0);
        
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create button check task");
            s_cfg.reset_enabled = false;
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "Reset button configured: GPIO=%d, active=%s, hold=%lums",
                 gpio_num, active_low ? "LOW" : "HIGH", (unsigned long)hold_ms);
    } else {
        s_cfg.reset_enabled = false;
        ESP_LOGI(TAG, "Reset button disabled");
    }
    
    return ESP_OK;
}

/**
 * @brief 手动触发重置
 */
esp_err_t wifi_config_trigger_reset(void)
{
    ESP_LOGW(TAG, "Manual reset triggered");
    set_state(WIFI_CONFIG_STATE_RESET);
    
    return wifi_config_reset_to_initial();
}

/**
 * @brief 启用 LED 状态指示
 */
esp_err_t wifi_config_enable_status_led(int gpio_num, bool active_low)
{
    s_cfg.led_gpio = gpio_num;
    s_cfg.led_active_low = active_low;
    
    if (gpio_num >= 0) {
        /* 配置 GPIO */
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << gpio_num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure LED GPIO: %s", esp_err_to_name(err));
            return err;
        }
        
        s_cfg.led_enabled = true;
        set_led(false);
        
        ESP_LOGI(TAG, "Status LED configured: GPIO=%d, active=%s",
                 gpio_num, active_low ? "LOW" : "HIGH");
    } else {
        s_cfg.led_enabled = false;
        ESP_LOGI(TAG, "Status LED disabled");
    }
    
    return ESP_OK;
}

/**
 * @brief 禁用 LED 状态指示
 */
void wifi_config_disable_status_led(void)
{
    s_cfg.led_enabled = false;
    if (s_cfg.led_timer) {
        xTimerStop(s_cfg.led_timer, 0);
    }
    set_led(false);
}
