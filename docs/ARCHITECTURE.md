# liteArm 架构说明

## 项目概述

liteArm 是基于分层架构的 ESP32 技能调用系统，专为嵌入式设备设计。系统支持命令格式的技能调用，内置飞书消息集成，提供 WiFi 配网和心跳监控功能。

## 核心特性

- **内置飞书集成**: 通过 WebSocket 接收和发送消息
- **技能系统**: 支持 `!技能名[参数]` 格式调用
- **WiFi 管理**: 自动重连、扫描、凭证持久化
- **WiFi 配网**: 物理按钮重置机制
- **心跳监控**: 定期发送系统状态报告
- **技能注册**: 可扩展的技能注册机制

## 系统架构

```
┌─────────────────────────────────────────────┐
│           应用层 (app_main)                  │
│  系统初始化、服务启动、CLI 管理               │
├─────────────────────────────────────────────┤
│           Agent 核心层                       │
│  ┌───────────────┐  ┌──────────────────┐   │
│  │ Command Parser│  │  Agent Core      │   │
│  │  命令解析      │  │  消息处理循环    │   │
│  └───────────────┘  └──────────────────┘   │
├─────────────────────────────────────────────┤
│           Skills 技能层                      │
│  ┌─────────────────────────────────────┐   │
│  │  Skill Registry (技能注册表)          │   │
│  └─────────────────────────────────────┘   │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐     │
│  │时间  │ │文件  │ │定时  │ │系统  │     │
│  └──────┘ └──────┘ └──────┘ └──────┘     │
├─────────────────────────────────────────────┤
│           渠道层                            │
│  ┌─────────────────────────────────────┐   │
│  │  Feishu Bot (WebSocket)              │   │
│  │  消息接收/发送、Token 管理           │   │
│  └─────────────────────────────────────┘   │
├─────────────────────────────────────────────┤
│           基础设施层                        │
│  ┌──────┐  ┌──────┐  ┌──────┐            │
│  │消息  │  │SPIFFS│  │ NVS  │            │
│  │队列  │  │存储  │  │配置  │            │
│  └──────┘  └──────┘  └──────┘            │
└─────────────────────────────────────────────┘
```

## 模块说明

### 1. Agent 核心层
- **command_parser**: 解析命令格式，提取技能名和参数
- **agent_core**: 消息处理循环，技能调度执行

### 2. Skills 技能层
- **skill_registry**: 技能注册表，支持动态注册和查找
- **内置技能**: 时间获取、文件操作、定时任务、系统管理

### 3. 渠道层
- **feishu_bot**: 飞书 WebSocket 连接，消息收发，Token 管理

### 4. 基础设施层
- **message_bus**: 双向消息队列（入站/出站）
- **SPIFFS**: 文件系统存储
- **NVS**: 非易失性配置存储

### 5. WiFi 管理
- **wifi_manager**: WiFi 连接管理，自动重连
- **wifi_config**: WiFi 配网，物理按钮重置

### 6. 心跳监控
- **heartbeat**: 定期发送系统状态，支持多渠道

## 技术栈

- **开发框架**: ESP-IDF v5.5+
- **操作系统**: FreeRTOS
- **硬件平台**: ESP32-S3 (推荐)
- **最小内存**: 2MB Flash
- **可选**: PSRAM (提升稳定性)

## 数据流

```
飞书消息 → WebSocket → 消息队列 → Agent Core → 技能执行 → 结果 → 消息队列 → 飞书
```

## 快速开始

详细使用说明请参考 [README.md](../README.md)

## 扩展开发

### 添加新技能

```c
// 1. 创建技能文件 skill_my_skill.c
#include "skill_my_skill.h"
#include "../skill_registry.h"
#include "esp_log.h"

static const char *TAG = "skill_my";

static esp_err_t skill_my_execute(const cJSON *params, skill_result_t *result) {
    // 实现技能逻辑
    result->success = true;
    snprintf(result->message, sizeof(result->message), "技能执行成功");
    return ESP_OK;
}

esp_err_t skill_my_register(void) {
    skill_def_t skill = {
        .name = "my_skill",
        .description = "我的技能",
        .category = "custom",
        .execute = skill_my_execute,
        .aliases = NULL,
        .alias_count = 0
    };
    return skill_register(&skill);
}

// 2. 在 litearm.c 中注册
#include "skills/builtin/skill_my_skill.h"
ESP_ERROR_CHECK(skill_my_register());

// 3. 更新 CMakeLists.txt
// 添加 skill_my_skill.c 到源文件列表
```

## 内置技能列表

| 技能名 | 描述 | 用法 |
|--------|------|------|
| get_time | 获取当前时间 | `!get_time` 或 `!时间` |
| file_ops | 文件读写操作 | `!file_ops[{"action":"read","path":"/spiffs/test.txt"}]` |
| cron | 定时任务管理 | `!cron[{"action":"list"}]` |
| get_status | 系统状态查询 | `!get_status` 或 `!系统` |
| help | 帮助信息 | `!help` 或 `?` |

## 性能指标

- **内存占用**: ~100KB RAM
- **Flash 占用**: ~1MB
- **启动时间**: < 3秒
- **响应延迟**: < 100ms

## 许可证

MIT License

## 作者

MtZhr  
GitHub: https://github.com/MtZhr/liteArm
