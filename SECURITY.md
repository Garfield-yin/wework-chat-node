# 安全策略

## 支持的版本

只有最新的 minor 版本会收到安全修复。

| 版本 | 是否支持 |
| ---- | -------- |
| 1.2.x | ✅ |
| < 1.2 | ❌ |

## 报告漏洞

**请不要用公开 issue 报告安全漏洞。**

请通过 GitHub 的 [Security Advisory](https://github.com/Garfield-yin/wework-chat-node/security/advisories/new)
私下提交。收到后会尽快确认并给出修复计划。

## 使用者须知

本模块处理的是企业微信会话存档数据，属于高度敏感信息，请注意：

- **私钥不要写进代码库。** 建议从环境变量或密钥管理服务读取。本仓库的
  `.gitignore` 已经忽略了 `demon.js`、`.env` 等常见的本地调试文件，但这只是兜底。
- **不要把私钥打进日志。** 本模块在解密失败时只会输出消息的 `seq` 和
  `publickey_ver`，不会输出私钥或密文内容。如果你自己加日志，请注意这一点。
- **解密后的消息是明文聊天记录。** 落库、传输、备份都应按敏感数据处理。
- **注意 `.so` 的来源。** `lib/` 下的 `libWeWorkFinanceSdk_C.so` 来自企业微信官方
  SDK 包。升级时请从[官方渠道](https://developer.work.weixin.qq.com/document/path/91774)
  下载，不要使用来路不明的二进制。
