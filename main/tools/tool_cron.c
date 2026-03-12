/**
 * @file tool_cron.c
 * @brief Cron Job Tools
 */

#include "tool_registry.h"
#include "litearm_config.h"
#include "message_bus.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "cron";

/* 最大定时任务数 */
#define CRON_MAX_JOBS       16

/* 任务类型 */
typedef enum {
    CRON_TYPE_EVERY,    /* 重复执行 */
    CRON_TYPE_ONCE,     /* 一次性 */
} cron_type_t;

/* 任务状态 */
typedef enum {
    CRON_STATUS_ACTIVE,
    CRON_STATUS_PAUSED,
    CRON_STATUS_EXPIRED,
} cron_status_t;

/* 任务结构 */
typedef struct {
    char id[16];            /* 任务 ID */
    char name[64];          /* 任务名称 */
    cron_type_t type;       /* 任务类型 */
    cron_status_t status;   /* 任务状态 */
    int interval_s;         /* 间隔秒数 (重复任务) */
    int64_t next_fire;      /* 下次触发时间 (微秒) */
    char message[256];      /* 触发消息 */
    char channel[32];       /* 响应渠道 */
    char chat_id[64];       /* 响应会话 */
    int fire_count;         /* 已触发次数 */
} cron_job_t;

/* 任务存储 */
static cron_job_t s_jobs[CRON_MAX_JOBS];
static int s_job_count = 0;
static TaskHandle_t s_cron_task = NULL;
static bool s_cron_running = false;

/* 外部引用 - 消息总线推送 */

/**
 * @brief 生成任务 ID
 */
static void generate_job_id(char *id, size_t size)
{
    int64_t now = esp_timer_get_time();
    snprintf(id, size, "%08llx", (unsigned long long)(now & 0xFFFFFFFF));
}

/**
 * @brief 计算下次触发时间
 */
static int64_t calc_next_fire(cron_job_t *job)
{
    int64_t now = esp_timer_get_time();
    
    if (job->type == CRON_TYPE_EVERY) {
        return now + (int64_t)job->interval_s * 1000000LL;
    } else {
        return job->next_fire;  /* 一次性任务使用预设时间 */
    }
}

/**
 * @brief Cron 任务调度器
 */
static void cron_scheduler_task(void *arg)
{
    (void)arg;
    
    ESP_LOGI(TAG, "Cron scheduler started");
    
    while (s_cron_running) {
        int64_t now = esp_timer_get_time();
        
        /* 检查所有任务 */
        for (int i = 0; i < s_job_count; i++) {
            cron_job_t *job = &s_jobs[i];
            
            if (job->status != CRON_STATUS_ACTIVE) continue;
            
            /* 检查是否到期 */
            if (now >= job->next_fire) {
                ESP_LOGI(TAG, "Job '%s' firing: %s", job->name, job->message);
                
                /* 推送消息到入站队列 */
                litearm_msg_t msg = {0};
                strncpy(msg.channel, job->channel[0] ? job->channel : LITEARM_CHAN_SYSTEM, 
                        sizeof(msg.channel) - 1);
                strncpy(msg.chat_id, job->chat_id, sizeof(msg.chat_id) - 1);
                msg.content = strdup(job->message);
                
                if (msg.content) {
                    message_bus_push_inbound_from_cron(&msg);
                }
                
                job->fire_count++;
                
                /* 计算下次触发时间 */
                if (job->type == CRON_TYPE_EVERY) {
                    job->next_fire = calc_next_fire(job);
                } else {
                    job->status = CRON_STATUS_EXPIRED;
                    ESP_LOGI(TAG, "Job '%s' expired (one-shot)", job->name);
                }
            }
        }
        
        /* 每秒检查一次 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Cron scheduler stopped");
    vTaskDelete(NULL);
}

/**
 * @brief 启动 Cron 调度器
 */
static esp_err_t cron_start_scheduler(void)
{
    if (s_cron_running) {
        return ESP_OK;
    }
    
    s_cron_running = true;
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        cron_scheduler_task,
        "cron_sched",
        4 * 1024,
        NULL,
        5,
        &s_cron_task,
        0);
    
    if (ret != pdPASS) {
        s_cron_running = false;
        ESP_LOGE(TAG, "Failed to create cron scheduler task");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief 添加定时任务
 */
esp_err_t tool_cron_add_execute(const char *params_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 解析参数 */
    cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
    if (!params) {
        snprintf(output, output_size, "错误: 参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 获取必要参数 */
    cJSON *name_json = cJSON_GetObjectItem(params, "name");
    cJSON *type_json = cJSON_GetObjectItem(params, "schedule_type");
    cJSON *msg_json = cJSON_GetObjectItem(params, "message");
    
    if (!name_json || !cJSON_IsString(name_json) ||
        !type_json || !cJSON_IsString(type_json) ||
        !msg_json || !cJSON_IsString(msg_json)) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 缺少必要参数 (name, schedule_type, message)");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *name = name_json->valuestring;
    const char *type_str = type_json->valuestring;
    const char *message = msg_json->valuestring;
    
    /* 检查任务数 */
    if (s_job_count >= CRON_MAX_JOBS) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 任务数已达上限 (%d)", CRON_MAX_JOBS);
        return ESP_ERR_NO_MEM;
    }
    
    /* 创建新任务 */
    cron_job_t *job = &s_jobs[s_job_count];
    memset(job, 0, sizeof(cron_job_t));
    
    generate_job_id(job->id, sizeof(job->id));
    strncpy(job->name, name, sizeof(job->name) - 1);
    strncpy(job->message, message, sizeof(job->message) - 1);
    
    /* 解析类型 */
    if (strcmp(type_str, "every") == 0) {
        job->type = CRON_TYPE_EVERY;
        
        cJSON *interval_json = cJSON_GetObjectItem(params, "interval_s");
        if (!interval_json || !cJSON_IsNumber(interval_json)) {
            cJSON_Delete(params);
            snprintf(output, output_size, "错误: 缺少 interval_s 参数");
            return ESP_ERR_INVALID_ARG;
        }
        job->interval_s = interval_json->valueint;
        
        if (job->interval_s < 1) {
            cJSON_Delete(params);
            snprintf(output, output_size, "错误: 间隔必须 >= 1 秒");
            return ESP_ERR_INVALID_ARG;
        }
    } else if (strcmp(type_str, "at") == 0) {
        job->type = CRON_TYPE_ONCE;
        
        cJSON *at_json = cJSON_GetObjectItem(params, "at_epoch");
        if (!at_json || !cJSON_IsNumber(at_json)) {
            cJSON_Delete(params);
            snprintf(output, output_size, "错误: 缺少 at_epoch 参数");
            return ESP_ERR_INVALID_ARG;
        }
        job->next_fire = (int64_t)at_json->valueint * 1000000LL;
    } else {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 未知的 schedule_type: %s", type_str);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 可选参数 */
    cJSON *channel_json = cJSON_GetObjectItem(params, "channel");
    if (channel_json && cJSON_IsString(channel_json)) {
        strncpy(job->channel, channel_json->valuestring, sizeof(job->channel) - 1);
    }
    
    cJSON *chat_id_json = cJSON_GetObjectItem(params, "chat_id");
    if (chat_id_json && cJSON_IsString(chat_id_json)) {
        strncpy(job->chat_id, chat_id_json->valuestring, sizeof(job->chat_id) - 1);
    }
    
    /* 计算下次触发时间 */
    job->next_fire = calc_next_fire(job);
    job->status = CRON_STATUS_ACTIVE;
    job->fire_count = 0;
    
    s_job_count++;
    
    /* 启动调度器 */
    cron_start_scheduler();
    
    cJSON_Delete(params);
    
    /* 格式化下次触发时间 */
    time_t next = (time_t)(job->next_fire / 1000000LL);
    struct tm *tm_info = localtime(&next);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    snprintf(output, output_size,
        "定时任务已添加:\n\n"
        "  ID: %s\n"
        "  名称: %s\n"
        "  类型: %s\n"
        "  间隔: %d 秒\n"
        "  下次触发: %s\n"
        "  消息: %s",
        job->id, job->name,
        job->type == CRON_TYPE_EVERY ? "重复" : "一次性",
        job->interval_s,
        time_str,
        job->message);
    
    ESP_LOGI(TAG, "Added cron job: %s (id=%s)", job->name, job->id);
    return ESP_OK;
}

/**
 * @brief 列出定时任务
 */
esp_err_t tool_cron_list_execute(const char *params_json, char *output, size_t output_size)
{
    (void)params_json;
    
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    if (s_job_count == 0) {
        snprintf(output, output_size, "当前没有定时任务");
        return ESP_OK;
    }
    
    int len = snprintf(output, output_size, "定时任务列表 (共 %d 个):\n\n", s_job_count);
    
    for (int i = 0; i < s_job_count && len < (int)output_size - 128; i++) {
        cron_job_t *job = &s_jobs[i];
        
        /* 计算下次触发时间 */
        time_t next = (time_t)(job->next_fire / 1000000LL);
        struct tm *tm_info = localtime(&next);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%m-%d %H:%M", tm_info);
        
        const char *status_str = "✅";
        if (job->status == CRON_STATUS_PAUSED) status_str = "⏸️";
        else if (job->status == CRON_STATUS_EXPIRED) status_str = "❌";
        
        len += snprintf(output + len, output_size - len,
            "%s [%s] %s\n"
            "    类型: %s | 间隔: %ds | 触发: %s\n"
            "    消息: %.32s%s\n\n",
            status_str, job->id, job->name,
            job->type == CRON_TYPE_EVERY ? "重复" : "一次性",
            job->interval_s, time_str,
            job->message, strlen(job->message) > 32 ? "..." : "");
    }
    
    return ESP_OK;
}

/**
 * @brief 删除定时任务
 */
esp_err_t tool_cron_remove_execute(const char *params_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    output[0] = '\0';
    
    /* 解析参数 */
    cJSON *params = cJSON_Parse(params_json ? params_json : "{}");
    if (!params) {
        snprintf(output, output_size, "错误: 参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *id_json = cJSON_GetObjectItem(params, "job_id");
    if (!id_json || !cJSON_IsString(id_json)) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 缺少 job_id 参数");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *job_id = id_json->valuestring;
    
    /* 查找并删除任务 */
    int found = -1;
    for (int i = 0; i < s_job_count; i++) {
        if (strcmp(s_jobs[i].id, job_id) == 0) {
            found = i;
            break;
        }
    }
    
    if (found < 0) {
        cJSON_Delete(params);
        snprintf(output, output_size, "错误: 未找到任务 ID: %s", job_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    char deleted_name[64];
    strncpy(deleted_name, s_jobs[found].name, sizeof(deleted_name) - 1);
    
    /* 移动数组 */
    for (int i = found; i < s_job_count - 1; i++) {
        s_jobs[i] = s_jobs[i + 1];
    }
    s_job_count--;
    
    cJSON_Delete(params);
    
    snprintf(output, output_size, "已删除定时任务: %s (ID: %s)", deleted_name, job_id);
    ESP_LOGI(TAG, "Removed cron job: %s", job_id);
    return ESP_OK;
}

/**
 * @brief 初始化 Cron 模块
 */
esp_err_t tool_cron_init(void)
{
    s_job_count = 0;
    memset(s_jobs, 0, sizeof(s_jobs));
    
    ESP_LOGI(TAG, "Cron module initialized");
    return ESP_OK;
}
