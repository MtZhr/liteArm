/**
 * @file tool_registry.c
 * @brief Tool Registry Implementation
 *
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#include "tool_registry.h"
#include "litearm_config.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "cJSON.h"

/* 工具执行函数声明 */
esp_err_t tool_get_time_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_read_file_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_write_file_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_edit_file_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_list_dir_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_cron_add_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_cron_list_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_cron_remove_execute(const char *params_json, char *output, size_t output_size);

/* 系统工具执行函数声明 */
esp_err_t tool_set_wifi_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_get_status_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_set_heartbeat_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_reboot_execute(const char *params_json, char *output, size_t output_size);
esp_err_t tool_wifi_scan_execute(const char *params_json, char *output, size_t output_size);

static const char *TAG = "tools";

/* 工具表 */
static litearm_tool_t s_tools[LITEARM_MAX_TOOLS];
static int s_tool_count = 0;
static char *s_tools_json = NULL;

/**
 * @brief 注册工具
 */
static void register_tool(const litearm_tool_t *tool)
{
    if (s_tool_count >= LITEARM_MAX_TOOLS) {
        ESP_LOGE(TAG, "Tool registry full");
        return;
    }
    s_tools[s_tool_count++] = *tool;
    ESP_LOGI(TAG, "Registered tool: %s", tool->name);
}

/**
 * @brief 构建工具 JSON 描述
 */
static void build_tools_json(void)
{
    cJSON *arr = cJSON_CreateArray();

    for (int i = 0; i < s_tool_count; i++) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", s_tools[i].name);
        cJSON_AddStringToObject(tool, "description", s_tools[i].description);

        cJSON *schema = cJSON_Parse(s_tools[i].param_schema);
        if (schema) {
            cJSON_AddItemToObject(tool, "input_schema", schema);
        }

        cJSON_AddItemToArray(arr, tool);
    }

    free(s_tools_json);
    s_tools_json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    ESP_LOGI(TAG, "Tools JSON built (%d tools)", s_tool_count);
}

/**
 * @brief 内部帮助工具执行
 */
static esp_err_t tool_help_execute(const char *params_json, char *output, size_t output_size)
{
    (void)params_json;
    
    int len = snprintf(output, output_size, 
        "=== liteArm 工具列表 ===\n\n");
    
    for (int i = 0; i < s_tool_count; i++) {
        /* 跳过 help 工具本身 */
        if (strcmp(s_tools[i].name, "help") == 0) continue;
        
        len += snprintf(output + len, output_size - len,
            "%-16s - %s\n",
            s_tools[i].name, 
            s_tools[i].description);
    }
    
    len += snprintf(output + len, output_size - len,
        "\n使用格式:\n"
        "  !工具名              - 执行无参数工具\n"
        "  !工具名[参数]        - 执行带参数工具\n"
        "  /工具名 参数         - 空格分隔格式\n"
        "\n示例:\n"
        "  !get_time\n"
        "  !read_file[{\"path\": \"/spiffs/config.txt\"}]\n"
        "  !write_file[{\"path\": \"/spiffs/test.txt\", \"content\": \"hello\"}]\n");
    
    return ESP_OK;
}

/**
 * @brief 初始化工具注册表
 */
esp_err_t tool_registry_init(void)
{
    s_tool_count = 0;

    /* 注册 help 工具 */
    litearm_tool_t help_tool = {
        .name = "help",
        .description = "显示帮助信息",
        .param_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        .execute = tool_help_execute,
    };
    register_tool(&help_tool);

    /* 注册 get_time */
    litearm_tool_t gt = {
        .name = "get_time",
        .description = "获取当前日期和时间",
        .param_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        .execute = tool_get_time_execute,
    };
    register_tool(&gt);

    /* 注册 read_file */
    litearm_tool_t rf = {
        .name = "read_file",
        .description = "读取文件内容",
        .param_schema = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"文件路径\"}},\"required\":[\"path\"]}",
        .execute = tool_read_file_execute,
    };
    register_tool(&rf);

    /* 注册 write_file */
    litearm_tool_t wf = {
        .name = "write_file",
        .description = "写入或创建文件",
        .param_schema = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"]}",
        .execute = tool_write_file_execute,
    };
    register_tool(&wf);

    /* 注册 edit_file */
    litearm_tool_t ef = {
        .name = "edit_file",
        .description = "编辑文件内容（替换文本）",
        .param_schema = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"old_string\":{\"type\":\"string\"},\"new_string\":{\"type\":\"string\"}},\"required\":[\"path\",\"old_string\",\"new_string\"]}",
        .execute = tool_edit_file_execute,
    };
    register_tool(&ef);

    /* 注册 list_dir */
    litearm_tool_t ld = {
        .name = "list_dir",
        .description = "列出目录中的文件",
        .param_schema = "{\"type\":\"object\",\"properties\":{\"prefix\":{\"type\":\"string\",\"description\":\"目录路径前缀\"}},\"required\":[]}",
        .execute = tool_list_dir_execute,
    };
    register_tool(&ld);

    /* 注册 cron_add */
    litearm_tool_t ca = {
        .name = "cron_add",
        .description = "添加定时任务",
        .param_schema = "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"schedule_type\":{\"type\":\"string\"},\"interval_s\":{\"type\":\"integer\"},\"message\":{\"type\":\"string\"}},\"required\":[\"name\",\"schedule_type\",\"message\"]}",
        .execute = tool_cron_add_execute,
    };
    register_tool(&ca);

    /* 注册 cron_list */
    litearm_tool_t cl = {
        .name = "cron_list",
        .description = "列出所有定时任务",
        .param_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        .execute = tool_cron_list_execute,
    };
    register_tool(&cl);

    /* 注册 cron_remove */
    litearm_tool_t cr = {
        .name = "cron_remove",
        .description = "删除定时任务",
        .param_schema = "{\"type\":\"object\",\"properties\":{\"job_id\":{\"type\":\"string\"}},\"required\":[\"job_id\"]}",
        .execute = tool_cron_remove_execute,
    };
    register_tool(&cr);

    /* ========== 系统工具 ========== */

    /* 注册 set_wifi - 通过飞书配置 WiFi */
    litearm_tool_t sw = {
        .name = "set_wifi",
        .description = "设置WiFi凭证 (需要重启生效)",
        .param_schema = "{\"type\":\"object\",\"properties\":{\"ssid\":{\"type\":\"string\",\"description\":\"WiFi名称\"},\"password\":{\"type\":\"string\",\"description\":\"WiFi密码\"}},\"required\":[\"ssid\",\"password\"]}",
        .execute = tool_set_wifi_execute,
    };
    register_tool(&sw);

    /* 注册 get_status - 获取系统状态 */
    litearm_tool_t gs = {
        .name = "get_status",
        .description = "获取系统状态 (内存/网络/队列)",
        .param_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        .execute = tool_get_status_execute,
    };
    register_tool(&gs);

    /* 注册 set_heartbeat - 设置心跳目标 */
    litearm_tool_t sh = {
        .name = "set_heartbeat",
        .description = "设置心跳监控目标",
        .param_schema = "{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"string\",\"description\":\"渠道(如feishu)\"},\"chat_id\":{\"type\":\"string\",\"description\":\"会话ID\"},\"interval_minutes\":{\"type\":\"integer\",\"description\":\"间隔分钟数\"}},\"required\":[\"channel\",\"chat_id\"]}",
        .execute = tool_set_heartbeat_execute,
    };
    register_tool(&sh);

    /* 注册 reboot - 重启设备 */
    litearm_tool_t rb = {
        .name = "reboot",
        .description = "重启设备 (需要确认)",
        .param_schema = "{\"type\":\"object\",\"properties\":{\"confirm\":{\"type\":\"string\",\"description\":\"确认重启,必须为yes\"}},\"required\":[\"confirm\"]}",
        .execute = tool_reboot_execute,
    };
    register_tool(&rb);

    /* 注册 wifi_scan - 扫描WiFi网络 */
    litearm_tool_t ws = {
        .name = "wifi_scan",
        .description = "扫描附近的WiFi网络",
        .param_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        .execute = tool_wifi_scan_execute,
    };
    register_tool(&ws);

    build_tools_json();

    ESP_LOGI(TAG, "Tool registry initialized with %d tools", s_tool_count);
    return ESP_OK;
}

/**
 * @brief 注册新工具
 */
esp_err_t tool_registry_register(const litearm_tool_t *tool)
{
    if (!tool || !tool->name || !tool->execute) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 检查是否已存在 */
    if (tool_registry_exists(tool->name)) {
        ESP_LOGW(TAG, "Tool already exists: %s", tool->name);
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_tool_count >= LITEARM_MAX_TOOLS) {
        ESP_LOGE(TAG, "Tool registry full");
        return ESP_ERR_NO_MEM;
    }
    
    register_tool(tool);
    build_tools_json();
    
    return ESP_OK;
}

/**
 * @brief 执行工具
 */
esp_err_t tool_registry_execute(const char *name, const char *params_json,
                                char *output, size_t output_size)
{
    if (!name || !output) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 处理 help 命令 */
    if (strcmp(name, "help") == 0) {
        return tool_help_execute(params_json, output, output_size);
    }
    
    /* 查找并执行工具 */
    for (int i = 0; i < s_tool_count; i++) {
        if (strcmp(s_tools[i].name, name) == 0) {
            ESP_LOGI(TAG, "Executing tool: %s", name);
            return s_tools[i].execute(params_json, output, output_size);
        }
    }
    
    ESP_LOGW(TAG, "Unknown tool: %s", name);
    snprintf(output, output_size, "错误: 未知工具 '%s'", name);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief 检查工具是否存在
 */
bool tool_registry_exists(const char *name)
{
    if (!name) return false;
    
    for (int i = 0; i < s_tool_count; i++) {
        if (strcmp(s_tools[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 获取工具 JSON 描述
 */
const char* tool_registry_get_tools_json(void)
{
    return s_tools_json;
}

/**
 * @brief 获取工具数量
 */
int tool_registry_get_count(void)
{
    return s_tool_count;
}
