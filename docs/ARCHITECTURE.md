# liteArm 完整项目实现说明

## 项目概述

liteArm 是基于 liteArm 分层架构的轻量级工具调用系统，专为资源受限的 ESP32 设计。它保留了原始项目的核心设计理念，但移除了 LLM 大模型依赖，改为基于命令格式的工具调用方式。

## 核心设计原则

### 1. 分层架构

```
┌────────────────────────────────────────────────────────────┐
│                     应用层 (app_main)                       │
│  系统初始化、服务启动、CLI 管理                              │
├────────────────────────────────────────────────────────────┤
│                    Agent 核心层                             │
│  ┌─────────────────┐  ┌─────────────────────────────────┐ │
│  │  Command Parser │  │  Agent Core (消息处理循环)       │ │
│  │  (命令解析)      │  │  - 入站队列消费                  │ │
│  │  - 格式识别      │  │  - 工具调度执行                  │ │
│  │  - 参数提取      │  │  - 出站队列推送                  │ │
│  └─────────────────┘  └─────────────────────────────────┘ │
├────────────────────────────────────────────────────────────┤
│                    业务渠道层                               │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  Feishu Bot (WebSocket 长连接)                       │  │
│  │  - 消息接收 (文本/图片/卡片)                          │  │
│  │  - 消息发送 (文本分段)                               │  │
│  │  - Token 管理                                       │  │
│  │  - 消息去重                                         │  │
│  └─────────────────────────────────────────────────────┘  │
├────────────────────────────────────────────────────────────┤
│                    工具执行层                               │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  Tool Registry (工具注册表)                          │  │
│  │  - 工具注册/查找                                     │  │
│  │  - 参数验证                                         │  │
│  │  - 执行调度                                         │  │
│  └─────────────────────────────────────────────────────┘  │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ │
│  │ get_time  │ │ file_ops  │ │   cron    │ │  (可扩展)  │ │
│  └───────────┘ └───────────┘ └───────────┘ └───────────┘ │
├────────────────────────────────────────────────────────────┤
│                    基础设施层                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐   │
│  │ Message Bus │  │   SPIFFS    │  │      NVS        │   │
│  │  消息队列    │  │  文件存储   │  │   配置存储      │   │
│  └─────────────┘  └─────────────┘  └─────────────────┘   │
└────────────────────────────────────────────────────────────┘
```

### 2. 消息流转机制

```
用户消息 (飞书)
    │
    ▼
飞书 WebSocket 接收
    │
    ▼
消息去重检查
    │
    ▼
Message Bus: inbound 队列
    │
    ▼
Agent Core: 消费入站消息
    │
    ▼
Command Parser: 解析命令
    │
    ├─── 是工具调用 ───► Tool Registry: 执行工具
    │                          │
    │                          ▼
    │                     构建响应
    │                          │
    └─── 非工具调用 ──────────►│
                               │
                               ▼
              Message Bus: outbound 队列
                               │
                               ▼
              出站分发任务: 按渠道发送
                               │
                               ▼
              飞书 API: 发送响应消息
```

### 3. 队列设计

**入站队列 (inbound_queue)**:
- 类型: FreeRTOS Queue
- 长度: 8 条消息
- 用途: 缓冲来自飞书的用户消息
- 优势: 削峰填谷，支持批量消息处理

**出站队列 (outbound_queue)**:
- 类型: FreeRTOS Queue
- 长度: 8 条消息
- 用途: 缓冲待发送的响应消息
- 优势: 异步发送，不阻塞主处理流程

## 模块详解

### 1. Command Parser (命令解析模块)

**支持的命令格式**:

| 格式 | 示例 | 说明 |
|------|------|------|
| `!工具名` | `!get_time` | 执行无参数工具 |
| `!工具名[JSON]` | `!read_file[{"path":"/spiffs/a.txt"}]` | 执行带 JSON 参数工具 |
| `/工具名` | `/help` | 斜杠开头格式 |
| `/工具名 参数` | `/list_dir /spiffs` | 空格分隔参数 |

**解析流程**:
1. 去除首尾空白
2. 检查是否以 `!` 或 `/` 开头
3. 提取工具名称
4. 提取参数 (JSON 格式或空格分隔)
5. 验证工具是否存在
6. 返回解析结果

### 2. Tool Registry (工具注册表)

**工具定义结构**:
```c
typedef struct {
    const char *name;           // 工具名称
    const char *description;    // 工具描述
    const char *param_schema;   // 参数 JSON Schema
    tool_execute_fn execute;    // 执行函数指针
} mimi_tool_t;
```

**注册流程**:
1. 调用 `tool_registry_register()` 注册工具
2. 工具被添加到静态数组
3. 自动生成工具 JSON 描述 (用于帮助信息)

**内置工具**:

| 工具名 | 功能 | 参数 |
|--------|------|------|
| `help` | 显示帮助 | 无 |
| `get_time` | 获取当前时间 | 无 |
| `read_file` | 读取文件 | path |
| `write_file` | 写入文件 | path, content |
| `edit_file` | 编辑文件 | path, old_string, new_string |
| `list_dir` | 列出目录 | prefix (可选) |
| `cron_add` | 添加定时任务 | name, schedule_type, message, interval_s |
| `cron_list` | 列出定时任务 | 无 |
| `cron_remove` | 删除定时任务 | job_id |

### 3. Feishu Bot (飞书渠道模块)

**WebSocket 协议**:
- 飞书使用自定义的 Protobuf 格式封装 WebSocket 帧
- 支持心跳 (ping/pong) 保活
- 支持事件推送 (im.message.receive_v1)

**消息处理流程**:
1. WebSocket 接收二进制帧
2. 解析 Protobuf 帧头和载荷
3. 提取事件 JSON
4. 解析消息内容
5. 推送到入站队列

**Token 管理**:
- 自动获取 tenant_access_token
- 缓存 Token 直到过期前 5 分钟
- 过期自动刷新

**消息分段发送**:
- 飞书单条消息限制 4096 字符
- 长消息自动分段发送

### 4. Agent Core (核心处理模块)

**任务架构**:
```
┌─────────────────┐     ┌─────────────────┐
│  Agent Loop     │     │ Outbound Task   │
│  (核心处理)      │     │  (响应分发)      │
│                 │     │                 │
│  消费 inbound   │     │  消费 outbound  │
│  解析命令       │────►│  调用飞书 API   │
│  执行工具       │     │  发送响应       │
└─────────────────┘     └─────────────────┘
```

**内存管理**:
- 消息内容动态分配
- 处理完成后立即释放
- 避免内存泄漏

## 内存优化

### 1. 栈空间优化

| 任务 | 原始大小 | 优化后大小 |
|------|----------|------------|
| Agent Loop | 24KB | 8KB |
| Outbound | 12KB | 6KB |
| Feishu WS | 12KB | 8KB |
| CLI | 4KB | 4KB |

### 2. 堆内存优化

- 使用静态缓冲区存储工具输出 (8KB)
- SPIFFS 限制最大文件 4KB
- 消息队列深度减半 (16 → 8)

### 3. Flash 空间优化

- 移除 LLM 相关代码 (~50KB)
- 移除 Telegram 模块 (~20KB)
- 精简系统提示模板
- 分区表优化: 
  - Factory App: 1.5MB
  - SPIFFS: 416KB

## 扩展指南

### 添加新工具

**步骤 1**: 实现工具函数
```c
// main/tools/tool_my_tool.c
#include "tool_registry.h"
#include "cJSON.h"

esp_err_t tool_my_tool_execute(const char *params_json, 
                                char *output, size_t output_size) {
    // 解析参数
    cJSON *params = cJSON_Parse(params_json);
    if (!params) {
        snprintf(output, output_size, "参数解析失败");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取参数
    cJSON *arg = cJSON_GetObjectItem(params, "arg_name");
    if (!arg || !cJSON_IsString(arg)) {
        snprintf(output, output_size, "缺少参数: arg_name");
        cJSON_Delete(params);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 执行逻辑
    const char *value = arg->valuestring;
    snprintf(output, output_size, "执行结果: %s", value);
    
    cJSON_Delete(params);
    return ESP_OK;
}
```

**步骤 2**: 在 `tool_registry_init()` 中注册
```c
mimi_tool_t my_tool = {
    .name = "my_tool",
    .description = "我的工具描述",
    .param_schema = "{\"type\":\"object\",\"properties\":{\"arg_name\":{\"type\":\"string\"}},\"required\":[\"arg_name\"]}",
    .execute = tool_my_tool_execute,
};
register_tool(&my_tool);
```

**步骤 3**: 在 CMakeLists.txt 中添加源文件
```cmake
SRCS
    ...
    "tools/tool_my_tool.c"
```

### 添加新消息类型支持

在 `command_parser.c` 中扩展:
```c
esp_err_t parse_feishu_message(const feishu_message_t *msg, parse_result_t *result) {
    switch (msg->type) {
        case MSG_TYPE_IMAGE:
            // 处理图片消息
            // 1. 下载图片
            // 2. 保存到 SPIFFS
            // 3. 返回路径信息
            break;
        // ...
    }
}
```

## 部署指南

### 编译和烧录

```bash
# 1. 设置目标芯片
idf.py set-target esp32s3

# 2. 配置密钥
cp main/mimi_secrets.h.example main/mimi_secrets.h
# 编辑 mimi_secrets.h

# 3. 编译
idf.py build

# 4. 烧录
idf.py -p /dev/ttyUSB0 flash

# 5. 监控
idf.py -p /dev/ttyUSB0 monitor
```

### 串口 CLI 配置

```
> set_wifi MyWiFi MyPassword
> set_feishu cli_xxx yyyyy
> restart
```

## 故障排除

### 问题 1: 飞书无法连接

**检查项**:
- WiFi 是否连接
- App ID / Secret 是否正确
- 网络是否可达 open.feishu.cn

### 问题 2: 命令无响应

**检查项**:
- 命令格式是否正确
- 工具名称是否正确
- 查看串口日志

### 问题 3: 内存不足

**解决方案**:
- 减少 SPIFFS 分区大小
- 启用 PSRAM
- 减少任务栈大小

## 性能指标

| 指标 | 数值 |
|------|------|
| 消息处理延迟 | < 100ms |
| 内存占用 (运行时) | ~80KB |
| Flash 占用 | ~600KB |
| 启动时间 | ~3s |

## 许可证

MIT License
