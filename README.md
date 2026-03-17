# liteArm

ESP32 工具调用系统，专为嵌入式设备设计。

## 特性

- **高效**: 无 LLM 依赖，适配 ESP32 2MB Flash
- **飞书集成**: 通过飞书 WebSocket 接收和发送消息
- **命令解析**: 支持 `!工具名[参数]` 格式调用工具
- **WiFi 管理**: 自动重连、扫描、凭证持久化
- **WiFi 配网**: 初始凭证配置 + 物理按钮重置
- **心跳监控**: 定期发送系统状态报告
- **可扩展**: 工具注册机制，方便添加新工具

## 快速开始

### 硬件要求

- ESP32-S3 开发板 (推荐)
- 2MB Flash (最低要求)
- 可选: PSRAM (提升稳定性)

### 使用方法

```bash
# 克隆项目
git clone https://github.com/MtZhr/liteArm.git
cd liteArm

# 安装 ESP-IDF v5.5+
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

# 配置密钥
cp main/litearm_secrets.h.example main/litearm_secrets.h
# 编辑 litearm_secrets.h 填写 WiFi 和飞书配置

# 编译烧录
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 配置

#### 通过串口 CLI

```
> set_wifi MyWiFi MyPassword        # 设置当前 WiFi
> set_initial_wifi InitWiFi InitPass # 设置初始 WiFi (用于重置)
> set_feishu cli_xxx yyyyy           # 设置飞书凭证
> reset_wifi                          # 手动重置到初始 WiFi
> restart
```

#### WiFi 配网机制

liteArm 支持两种 WiFi 凭证配置：

| 凭证类型 | 说明 | 设置方式 |
|----------|------|----------|
| **当前 WiFi** | 设备当前连接的网络 | `set_wifi` 或飞书 `!set_wifi` |
| **初始 WiFi** | 可恢复的默认网络 | `set_initial_wifi` 或编译时配置 |

**物理按钮重置**：
- 默认使用 GPIO0 (BOOT 按钮)
- 长按 5 秒自动恢复到初始 WiFi
- 重置后设备自动重启

**配置流程示例**：
```
1. 编译时设置初始 WiFi (litearm_secrets.h)
2. 通过飞书临时切换到其他 WiFi: !set_wifi[{"ssid":"Temp","password":"xxx"}]
3. 如需恢复，长按 BOOT 按钮 5 秒
4. 设备自动恢复到初始 WiFi 并重启
```

#### 飞书配置 WiFi

```
# 扫描附近网络
!wifi_scan

# 设置当前 WiFi
!set_wifi[{"ssid": "MyWiFi", "password": "MyPassword"}]

# 重启生效
!reboot[{"confirm": "yes"}]
```

## 命令格式

```
格式1: !工具名
格式2: !工具名[参数JSON]
格式3: /工具名 参数

示例:
  !help
  !get_time
  !read_file[{"path": "/spiffs/config.txt"}]
  !write_file[{"path": "/spiffs/test.txt", "content": "hello"}]
  !list_dir[{"prefix": "/spiffs"}]
```

## 可用工具

### 文件操作

| 工具名 | 描述 | 参数 |
|--------|------|------|
| help | 显示帮助 | 无 |
| get_time | 获取当前时间 | 无 |
| read_file | 读取文件 | path |
| write_file | 写入文件 | path, content |
| edit_file | 编辑文件 | path, old_string, new_string |
| list_dir | 列出目录 | prefix (可选) |

### 定时任务

| 工具名 | 描述 | 参数 |
|--------|------|------|
| cron_add | 添加定时任务 | name, schedule_type, message, interval_s/at_epoch |
| cron_list | 列出定时任务 | 无 |
| cron_remove | 删除定时任务 | job_id |

### 系统工具 (通过飞书配置)

| 工具名 | 描述 | 参数 |
|--------|------|------|
| set_wifi | 设置WiFi凭证 | ssid, password |
| get_status | 获取系统状态 | 无 |
| set_heartbeat | 设置心跳目标 | channel, chat_id, interval_minutes (可选) |
| wifi_scan | 扫描WiFi网络 | 无 |
| reboot | 重启设备 | confirm: "yes" |

### 飞书配置 WiFi 示例

```
# 扫描附近的 WiFi
!wifi_scan

# 设置 WiFi 凭证
!set_wifi[{"ssid": "MyWiFi", "password": "MyPassword"}]

# 查看系统状态
!get_status

# 重启设备 (需要确认)
!reboot[{"confirm": "yes"}]

# 设置心跳监控
!set_heartbeat[{"channel": "feishu", "chat_id": "ou_xxxxx", "interval_minutes": 60}]
```

## 串口 CLI 命令

| 命令 | 描述 |
|------|------|
| `help` | 显示帮助 |
| `status` | 显示系统状态 |
| `memory` | 显示内存信息 |
| `set_wifi <ssid> <pass>` | 设置 WiFi 凭证 |
| `set_initial_wifi <s> <p>` | 设置初始 WiFi |
| `reset_wifi` | 重置到初始 WiFi |
| `set_feishu <id> <secret>` | 设置飞书凭证 |
| `heartbeat <cmd>` | 心跳命令 (见下文) |
| `scan` | 扫描 WiFi 网络 |
| `tools` | 列出已注册工具 |
| `restart` | 重启设备 |

## 心跳监控

心跳功能定期向指定渠道发送系统状态报告。

### 配置心跳

```
# 启用心跳到飞书用户
> heartbeat enable feishu:ou_xxxxxx

# 设置心跳间隔 (分钟)
> heartbeat interval 30

# 立即触发心跳
> heartbeat now

# 查看心跳状态
> heartbeat status

# 停止心跳
> heartbeat stop
```

### 心跳消息内容

心跳消息包含以下信息：
- 当前时间
- 心跳次数统计
- 内存使用情况
- WiFi 连接状态
- IP 地址

## 项目结构

```
main/
├── litearm.c                 # 主入口
├── litearm_config.h          # 配置定义
├── bus/
│   ├── message_bus.c         # 消息队列
│   └── message_bus.h
├── wifi/
│   ├── wifi_manager.c        # WiFi 管理
│   ├── wifi_config.c         # WiFi 配网
│   └── *.h
├── channels/
│   └── feishu/
│       ├── feishu_bot.c      # 飞书 WebSocket
│       └── feishu_bot.h
├── agent/
│   ├── command_parser.c      # 命令解析
│   ├── agent_core.c          # 核心处理
│   └── *.h
├── tools/
│   ├── tool_registry.c       # 工具注册
│   ├── tool_get_time.c       # 时间工具
│   ├── tool_files.c          # 文件工具
│   ├── tool_cron.c           # 定时任务工具
│   └── tool_system.c         # 系统工具
└── heartbeat/
    ├── heartbeat.c           # 心跳监控
    └── heartbeat.h
```

## 架构

```
┌─────────────────────────────────────────────┐
│              飞书 WebSocket                  │
└──────────────────┬──────────────────────────┘
                   │ 消息
                   ▼
┌─────────────────────────────────────────────┐
│           Message Bus (队列)                │
│        inbound ←──→ outbound               │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│           Agent Core                        │
│      消息调度 + 工具执行                    │
└──────────────────┬──────────────────────────┘
                   │
        ┌──────────┴──────────┐
        ▼                     ▼
┌───────────────┐    ┌───────────────┐
│ Tool Registry │    │   Heartbeat   │
│   工具执行     │    │   心跳监控    │
└───────────────┘    └───────────────┘
```

## 添加新工具

```c
// 1. 实现工具函数
esp_err_t tool_my_tool_execute(const char *params_json, 
                                char *output, size_t output_size) {
    // 解析参数
    cJSON *params = cJSON_Parse(params_json);
    // 执行逻辑
    snprintf(output, output_size, "结果");
    cJSON_Delete(params);
    return ESP_OK;
}

// 2. 在 tool_registry_init() 中注册
litearm_tool_t my_tool = {
    .name = "my_tool",
    .description = "我的工具",
    .param_schema = "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
    .execute = tool_my_tool_execute,
};
register_tool(&my_tool);
```

## 故障排除

### WiFi 无法连接

1. 检查 SSID 和密码是否正确
2. 使用 `scan` 命令扫描附近网络
3. 检查路由器是否支持 2.4GHz (ESP32 不支持 5GHz)

### 飞书无法接收消息

1. 检查 App ID 和 Secret 是否正确
2. 确认飞书应用已启用机器人能力
3. 检查事件订阅是否配置正确

### 内存不足

1. 减少消息队列深度
2. 禁用不需要的功能
3. 启用 PSRAM

## 许可证

MIT License

## 作者

MtZhr  
GitHub: https://github.com/MtZhr/liteArm
