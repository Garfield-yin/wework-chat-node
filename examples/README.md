# 示例

运行前先设置凭据（**不要把私钥写进代码**）：

```bash
export WEWORK_CORPID=ww00000000000000
export WEWORK_SECRET=your-chat-archive-secret
export WEWORK_PRIVATE_KEY="$(cat /path/to/private_key.pem)"
```

因为企业微信只提供 Linux 版 SDK，示例只能在 Linux 上运行。

| 示例 | 说明 |
| ---- | ---- |
| [`batch-pull.js`](batch-pull.js) | 增量批量拉取，演示 seq 的正确推进方式 |
| [`stream.js`](stream.js) | 后台持续轮询，以及如何优雅停止 |
| [`download-media.js`](download-media.js) | 分片下载媒体文件并写入磁盘 |

```bash
node examples/batch-pull.js
```
