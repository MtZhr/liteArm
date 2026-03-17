# liteArm

ESP32 Skills 系统，专为嵌入式设备设计。

## 特性

- **高效**: 内置技能系统，适配 ESP32 2MB Flash
- **飞书集成**: 通过飞书 WebSocket 接收和发送消息
- **技能系统**: 支持 `!技能名[参数]` 格式调用技能
- **WiFi 管理**: 自动重连、扫描、凭证持久化
- **WiFi 配网**: 初始凭证配置 + 物理按钮重置
- **心跳监控**: 定期发送系统状态报告
- **可扩展**: 技能注册机制，方便添加新技能

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
```

## 内置技能

| 技能名 | 别名 | 说明 |
|--------|------|------|
| `get_time` | `时间`, `time`, `now` | 获取当前系统时间 |
| `file_ops` | `文件`, `file` | 文件读取/写入操作 |
| `cron` | `定时`, `schedule` | 定时任务管理 |
| `get_status` | `系统`, `status` | 获取系统状态信息 |
| `help` | `帮助`, `?` | 显示所有可用技能 |

### 使用示例

```
!get_time
!file_ops[{"action":"read","path":"/spiffs/config.txt"}]
!file_ops[{"action":"write","path":"/spiffs/test.txt","content":"hello"}]
!cron_list
!get_status
!help
```

## 架构

```
┌─────────────────────────────────────────┐
│        应用层 (app_main)                 │
├─────────────────────────────────────────┤
│        Agent 核心层                      │
│  命令解析 | 消息处理 | 技能调度           │
├─────────────────────────────────────────┤
│        Skills 技能层                     │
│  技能注册 | 技能执行 | 结果返回           │
├─────────────────────────────────────────┤
│        渠道层                            │
│  飞书 WebSocket | 消息收发               │
├─────────────────────────────────────────┤
│        基础设施层                        │
│  消息队列 | SPIFFS | NVS | WiFi         │
└─────────────────────────────────────────┘
```

## 开发

### 添加新技能

1. 创建技能文件 `main/skills/builtin/skill_xxx.c/h`
2. 实现技能执行函数
3. 注册技能到系统
4. 更新 CMakeLists.txt

详见 [架构说明](docs/ARCHITECTURE.md)

## 许可证

MIT License

## 作者

MtZhr  
GitHub: https://github.com/MtZhr/liteArm
