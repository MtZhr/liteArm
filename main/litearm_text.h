/**
 * @file litearm_text.h
 * @brief liteArm 文本统一管理
 * 
 * @author MtZhr
 * @see https://github.com/MtZhr/liteArm
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 错误消息 ❌
 * ============================================================================ */

#define TXT_ERR_MEMORY              "❌ 内存不足"
#define TXT_ERR_SKILL_NOT_FOUND     "❌ 未找到技能"
#define TXT_ERR_PARAMS_MISSING      "❌ 缺少必要参数"
#define TXT_ERR_PARAMS_INVALID      "❌ 参数无效"
#define TXT_ERR_FILE_OPEN           "❌ 无法打开文件: %s"
#define TXT_ERR_FILE_CREATE         "❌ 无法创建文件: %s"
#define TXT_ERR_ACTION_UNKNOWN      "❌ 未知操作: %s"
#define TXT_ERR_TIME_NOT_SYNC       "⚠️ 时间未同步，请检查网络连接"
#define TXT_ERR_NOT_IMPLEMENTED     "⚠️ 功能未实现"

/* ============================================================================
 * 状态消息 ✅
 * ============================================================================ */

#define TXT_MSG_WELCOME             "👋 欢迎使用 liteArm!\n\n📝 发送 !help 查看可用技能\n💡 格式: !技能名[参数]\n\n示例:\n  !get_time\n  !file_ops[{\"action\":\"read\",\"path\":\"/spiffs/config.txt\"}]"

#define TXT_MSG_TIME_FORMAT         "🕐 当前时间: %s"
#define TXT_MSG_FILE_READ_OK        "✅ 文件读取成功 (%d 字节)"
#define TXT_MSG_FILE_WRITE_OK       "✅ 写入成功: %d 字节"
#define TXT_MSG_SYSTEM_STATUS       "📊 系统状态:\n版本: v%s\n内存: %d/%d bytes\n项目: https://github.com/MtZhr/liteArm"
#define TXT_MSG_CRON_LIST           "⏰ 定时任务列表功能（待实现）"
#define TXT_MSG_SKILL_LIST          "📋 可用技能 (%d):\n"

/* ============================================================================
 * 消息类型 📨
 * ============================================================================ */

#define TXT_MSG_TYPE_IMAGE          "🖼️ [图片消息]"
#define TXT_MSG_TYPE_VOICE          "🎤 [语音消息]"
#define TXT_MSG_TYPE_FILE           "📁 [文件消息]"
#define TXT_MSG_TYPE_CARD           "🃏 [卡片消息]"
#define TXT_MSG_TYPE_UNKNOWN        "❓ [未知消息类型]"

/* ============================================================================
 * 帮助文本 📚
 * ============================================================================ */

#define TXT_HELP_HEADER             "📚 === liteArm 帮助 ===\n\n"
#define TXT_HELP_FORMAT             "💡 命令格式:\n  !技能名              - 执行无参数技能\n  !技能名[参数]        - 执行带参数技能\n  /技能名 参数         - 空格分隔格式\n\n"
#define TXT_HELP_AVAILABLE          "🛠️ 可用技能:\n  get_time        - 🕐 获取当前时间\n  file_ops        - 📁 文件操作\n  cron            - ⏰ 定时任务\n  get_status      - 📊 系统状态\n  help            - 📚 帮助信息\n\n"
#define TXT_HELP_EXAMPLES           "📝 示例:\n  !get_time\n  !file_ops[{\"action\":\"read\",\"path\":\"/spiffs/config.txt\"}]\n  !help\n"

/* ============================================================================
 * CLI 文本 💻
 * ============================================================================ */

#define TXT_CLI_PROMPT              "💻 > "
#define TXT_CLI_SKILLS_HEADER       "📋 Registered Skills (%d):\n\n"
#define TXT_CLI_AVAILABLE_CMDS      "💡 Available commands:\n"
#define TXT_CLI_CMD_SKILLS          "skills"
#define TXT_CLI_CMD_STATUS          "status"
#define TXT_CLI_CMD_RESTART         "restart"
#define TXT_CLI_CMD_HELP            "help"
#define TXT_CLI_CMD_EXIT            "exit"
#define TXT_CLI_RESTARTING          "🔄 Restarting...\n"

#define TXT_CLI_HELP_SKILLS         "  skills                      - 📋 List registered skills"
#define TXT_CLI_HELP_STATUS         "  status                       - 📊 Show system status"
#define TXT_CLI_CMD_RESTART_HELP    "  restart                      - 🔄 Restart system"
#define TXT_CLI_CMD_HELP_HELP       "  help                         - 📚 Show this help"

/* ============================================================================
 * 心跳消息 💓
 * ============================================================================ */

#define TXT_HEARTBEAT_STATUS        "💓 系统状态: 内存 %d bytes, 运行时间 %lld 秒"

/* ============================================================================
 * 技能描述 🎯
 * ============================================================================ */

#define TXT_SKILL_GET_TIME          "🕐 获取当前系统时间"
#define TXT_SKILL_FILE_OPS          "📁 文件操作：读取/写入文件"
#define TXT_SKILL_CRON              "⏰ 定时任务管理"
#define TXT_SKILL_SYSTEM            "📊 获取系统状态信息"
#define TXT_SKILL_HELP              "📚 显示所有可用技能"

/* ============================================================================
 * 日志标签 🏷️
 * ============================================================================ */

#define TXT_TAG_AGENT_CORE          "agent_core"
#define TXT_TAG_CMD_PARSER          "cmd_parser"
#define TXT_TAG_SKILL_REGISTRY      "skill_registry"
#define TXT_TAG_SKILL_TIME          "skill_time"
#define TXT_TAG_SKILL_FILE          "skill_file"
#define TXT_TAG_SKILL_CRON          "skill_cron"
#define TXT_TAG_SKILL_SYSTEM        "skill_sys"
#define TXT_TAG_SKILL_HELP          "skill_help"
#define TXT_TAG_HEARTBEAT           "heartbeat"
#define TXT_TAG_FEISHU              "feishu_bot"
#define TXT_TAG_WIFI_MANAGER        "wifi_manager"
#define TXT_TAG_WIFI_CONFIG         "wifi_config"
#define TXT_TAG_MESSAGE_BUS         "message_bus"

#ifdef __cplusplus
}
#endif
