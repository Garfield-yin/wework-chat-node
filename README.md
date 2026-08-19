# wework-chat-node

[![CI](https://github.com/Garfield-yin/wework-chat-node/actions/workflows/ci.yml/badge.svg)](https://github.com/Garfield-yin/wework-chat-node/actions/workflows/ci.yml)
[![npm version](https://img.shields.io/npm/v/wework-chat-node.svg)](https://www.npmjs.com/package/wework-chat-node)
[![node](https://img.shields.io/node/v/wework-chat-node.svg)](https://www.npmjs.com/package/wework-chat-node)
[![license](https://img.shields.io/npm/l/wework-chat-node.svg)](LICENSE)

使用 [node-addon-api](https://github.com/nodejs/node-addon-api) 封装企业微信
**会话内容存档**（金融版）SDK，让 Node.js 可以直接拉取和解密会话记录。

[English](README.en.md) · [API 文档](docs/API.md) · [常见问题](docs/FAQ.md) · [更新日志](CHANGELOG.md)

---

## 特性

- 拉取并自动解密会话记录（RSA 解密 + SDK 解密一步到位）
- 支持一次性批量拉取（`getChatData`）和后台持续轮询（`fetchData`）两种模式
- 支持媒体文件（图片 / 语音 / 视频 / 文件）的分片下载
- 完整的 TypeScript 类型定义
- 同时提供 x86_64 与 arm64 的 SDK

## 运行环境

> [!IMPORTANT]
> **只支持 Linux。** 企业微信官方只提供 Linux 版的 `.so`，因此本模块无法在
> macOS / Windows 上运行。`npm install` 在非 Linux 平台会自动跳过编译步骤，
> 安装不会失败，但模块不可用。本地开发请使用 Docker。

| 项目 | 要求 |
| ---- | ---- |
| 操作系统 | Linux（x86_64 / arm64） |
| Node.js | >= 18，CI 覆盖 18 / 20 / 22 / 24 |
| 编译工具 | `python3`、`make`、`g++`（安装时需要现场编译） |

## 安装

```bash
npm install wework-chat-node
```

Debian / Ubuntu 上如果缺少编译工具：

```bash
sudo apt-get install -y python3 build-essential
```

## 快速开始

```javascript
const { WeWorkChat } = require("wework-chat-node");

const wework = new WeWorkChat({
	corpid: process.env.WEWORK_CORPID,
	secret: process.env.WEWORK_SECRET,
	// PEM 格式，含首尾行；不要硬编码在代码里
	private_key: process.env.WEWORK_PRIVATE_KEY,
	seq: 0,
});

const ret = wework.getChatData({ max_results: 1000, timeout: 30, seq: 0 });

for (const msg of ret.data) {
	if (!msg) continue; // 解密失败的消息会留空
	const item = JSON.parse(msg);
	console.log(item.msgid, item.msgtype, item.from);
}

console.log("本批最后一条 seq:", ret.last_seq);
wework.stopFetch(); // 释放 SDK
```

### 持续拉取

```javascript
wework.fetchData((msg) => {
	const item = JSON.parse(msg);
	console.log(item.msgid, item.msgtype);
});

process.on("SIGTERM", () => {
	const finalSeq = wework.stopFetch(); // 阻塞直到后台线程退出
	saveSeq(finalSeq);
});
```

### 增量轮询

`last_seq` 在本批为空时是 `0`，不要无条件赋值回去，否则会从头重拉：

```javascript
let seq = await loadSeq();

while (true) {
	const ret = wework.getChatData({ max_results: 1000, timeout: 30, seq });
	if (!ret.data.length) break;

	for (const msg of ret.data) {
		if (!msg) continue;
		await handle(JSON.parse(msg));
	}

	seq = ret.last_seq; // 只在有数据时推进
	await saveSeq(seq); // 落库，重启后从这里继续
}
```

### 下载媒体文件

```javascript
const fs = require("fs");

function download(fileName, sdkFileId) {
	const stream = fs.createWriteStream(fileName);
	let indexBuf = "";
	while (true) {
		const resp = wework.getMediaData({ sdk_fileid: sdkFileId, index_buf: indexBuf });
		stream.write(Buffer.from(resp.data));
		if (resp.is_finished) break;
		indexBuf = resp.buf_index; // 分片续传
	}
	stream.end();
}
```

大文件请像上面这样逐片写入，不要把所有分片攒在内存里再 `Buffer.concat`。

### TypeScript

自带类型定义，无需额外安装 `@types`：

```typescript
import { WeWorkChat, ChatDataItem, GetDataParams } from "wework-chat-node";

const params: GetDataParams = { max_results: 100, timeout: 30, seq: 0 };
const ret = wework.getChatData(params);

for (const msg of ret.data) {
	if (!msg) continue;
	const item: ChatDataItem = JSON.parse(msg);
	if (item.msgtype === "file" && item.file) {
		console.log(item.file.filename);
	}
}
```

## API 概览

| 方法 | 说明 |
| ---- | ---- |
| `new WeWorkChat(options)` | 创建实例并初始化 SDK |
| `getChatData(params)` | 同步拉取一批会话记录并解密 |
| `fetchData(callback)` | 启动后台线程持续轮询，每条消息回调一次 |
| `stopFetch()` | 停止轮询并释放 SDK，返回最终的 seq（阻塞） |
| `getMediaData(params[, cb])` | 拉取媒体文件，支持分片 |

完整参数、返回值和异常说明见 **[API 文档](docs/API.md)**。

## 安全提示

- **不要把私钥或 secret 写进代码库。** 请从环境变量或密钥管理服务读取。
- **解密后的消息是明文聊天记录**，落库、传输、备份都应按敏感数据处理。
- 本模块在解密失败时只输出 `seq` 和 `publickey_ver`，不会输出私钥。
  如果你自己加日志，请注意不要泄露。

详见 [SECURITY.md](SECURITY.md)。

## 遇到问题？

- 崩溃、编译失败、`max_results` 不生效、消息解密不出来 → **[常见问题](docs/FAQ.md)**
- 接口语义、错误码 → [企业微信官方文档](https://developer.work.weixin.qq.com/document/path/91774)
- 还是没解决 → [提 issue](https://github.com/Garfield-yin/wework-chat-node/issues/new/choose)

> 如果你正在使用 v1.2.1 之前的版本，**强烈建议升级** —— 早期版本存在多处会导致
> 进程 core dump 和内存无上限增长的问题，详见 [CHANGELOG](CHANGELOG.md)。

## 开发

```bash
npm install      # 安装依赖并编译 addon（仅 Linux）
npm run build    # 重新编译
npm test         # 运行测试（不需要凭据，不联网）
npm run test:docker  # 在 Docker 里跑，macOS / Windows 用这个
```

参与贡献请看 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

[MIT](LICENSE) © Garfield Yin
