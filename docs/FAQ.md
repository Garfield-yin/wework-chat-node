# 常见问题与故障排查

## 安装与编译

### macOS / Windows 上装不了，或者装上了用不了

**这是预期行为。** 企业微信只提供 Linux 版的会话存档 SDK
（仓库里的 `lib/x86/libWeWorkFinanceSdk_C.so` 和 `lib/arm/`），所以本模块
**只能在 Linux 上运行**。

`npm install` 里的编译步骤做了平台判断，在非 Linux 平台会跳过
`node-gyp rebuild`，安装本身不会失败，但模块无法使用。

本地开发请用 Docker：

```bash
docker run --rm -it -v "$PWD":/src -w /w node:24 bash
cp -r /src/. /w && npm install && npm test
```

### 编译报错 `OPENSSL_API_COMPAT expresses an impossible API compatibility level`

Node 17 开始内置 OpenSSL 3.0，其 `macros.h` 会校验 API 兼容级别，
推导结果与实际配置不符时就报这个错。

v1.2.1 起已在 `binding.gyp` 中显式固定：

```
"defines": [
  "NAPI_DISABLE_CPP_EXCEPTIONS",
  "OPENSSL_API_COMPAT=0x10100000L"
]
```

升级到 v1.2.1 或更高版本即可。

### 编译报错找不到 `python` / `make` / `g++`

`node-gyp` 需要这些工具：

```bash
# Debian / Ubuntu
sudo apt-get install -y python3 build-essential

# CentOS / RHEL
sudo yum install -y python3 gcc-c++ make
```

---

## 运行时崩溃

### `Segmentation fault (core dumped)`

**先升级到 v1.2.1 或更高版本。** 早期版本有多处会直接 core dump，
都已修复（见 [#6]）：

| 触发条件 | 原因 |
| -------- | ---- |
| 服务端返回 `errcode:0` 但没有 `chatdata` 字段 | 拉到最新时的正常响应；rapidjson 在 Release 构建下 `operator[]` 不做成员检查，直接野指针解引用。**这是线上最容易撞到的一种** |
| 重复调用 `stopFetch()` | sdk 被 `DestroySdk` 两次，double free |
| `stopFetch()` 之后后台线程还在跑 | 旧版只 sleep 800ms 就释放 sdk，而一次 `GetChatData` 最长 30s、一批 1000 条消息分发要 80s |
| 参数类型不对（如 `getChatData()` 不传参数） | `NAPI_DISABLE_CPP_EXCEPTIONS` 下抛 JS 异常不会中断 C++ 执行流，旧版抛完没有 `return` |
| 初始化失败后继续调用 | 旧版留下半初始化的 sdk 指针 |
| JS 侧没有变量引用住实例 | 后台线程持有 `this`，对象被 GC 回收后就是 use-after-free |

如果升级后仍然崩溃，请按下面的方法抓 backtrace 后提 issue：

```bash
ulimit -c unlimited
node your-script.js          # 复现崩溃
gdb node core.xxxx
(gdb) bt
```

同时说明崩溃发生在哪个调用之后（`getChatData` / `fetchData` / `stopFetch`）。

---

## 数据问题

### `max_results` 传了没用，每次都返回 1000 条

v1.2.1 之前的 bug（[#5]）：参数被读进了一个同名遮蔽的无用字符串，
实际调用 SDK 时用的是常量 `1000`。升级到 v1.2.1 即可。

### 怎么拿到最终的 seq

两种方式：

1. **`stopFetch()` 的返回值。** 它会阻塞等待后台线程真正退出（最长 45s）
   后再返回，所以拿到的是本次拉取真正处理完的最后一个 seq。

2. **`getChatData()` 返回的 `last_seq`。** 注意本批为空时它是 `0`，
   轮询时不要无条件赋值回去：

   ```javascript
   let seq = 0;
   while (true) {
   	const ret = wework.getChatData({ max_results: 1000, timeout: 30, seq });
   	if (!ret.data.length) break;
   	seq = ret.last_seq; // 只在有数据时推进
   	// ... 处理 ret.data
   }
   ```

**生产环境建议不要依赖 `stopFetch()` 的返回值持久化 seq** —— 进程被
`kill -9` 时它根本不会执行。更可靠的做法：

- 每处理完一条消息就把 seq 写入数据库或 Redis，重启后从落库的 seq 继续；
- 或者允许少量重复，接收端按 `msgid` 去重。

### 拿到的 `data` 数组里有空值 / 有些消息解密不出来

解密失败的消息会在 `data` 里留空（保持与原始批次的下标对应），遍历时请跳过：

```javascript
for (const msg of ret.data) {
	if (!msg) continue;
	const item = JSON.parse(msg);
}
```

**最常见的原因是私钥版本不匹配。** 企业微信允许存在多个版本的密钥，
每条消息的 `publickey_ver` 指明它该用哪个版本的私钥解密。

v1.2.1 起，解密失败时会打印出该条消息的 `seq` 和 `publickey_ver`：

```
WEWORK_CHAT_NODE::decrypt encrypt_random_key failed, seq:196 publickey_ver:3, 请确认 private_key 是该 publickey_ver 对应版本的私钥
```

按提示换成对应版本的私钥即可。如果你的企业轮换过密钥，历史消息可能需要用
旧版本私钥才能解密。

### 内存一直涨

v1.2.1 之前 `getChatData` 的每个错误分支、`fetchData` 轮询循环的两个
`continue` 分支都会泄漏 `Slice_t`（[#7]、[#8]）。`fetchData` 是长驻轮询，
实测在后端持续报错的情况下每秒泄漏约 7 个且无上限。升级到 v1.2.1 即可。

---

## 其它

### `fetchData` 和 `getChatData` 该用哪个

- `getChatData` 是同步拉取一批，适合自己控制节奏的定时任务、
  或配合企业微信的消息推送回调按需拉取。
- `fetchData` 会起后台线程持续轮询，适合需要准实时消费的场景。

企业微信现在支持会话存档消息推送，所以不再必须使用循环拉取的方式。

### 同一个实例可以同时跑多个 `fetchData` 吗

不可以。重复调用会抛异常。旧版本不会报错，但会起多个线程同时改写内部的 seq，
而且只有一个线程会被正确回收。需要重启拉取时请先 `stopFetch()`。

### 怎么升级内置的企业微信 SDK

按架构替换这几个文件后重新构建：

- `lib/x86/libWeWorkFinanceSdk_C.so` 或 `lib/arm/libWeWorkFinanceSdk_C.so`
- `include/wework/x86/WeWorkFinanceSdk_C.h` 或 `include/wework/arm/WeWorkFinanceSdk_C.h`

```bash
npm run build && npm test
```

请从[官方渠道](https://developer.work.weixin.qq.com/document/path/91774)下载 SDK。

[#5]: https://github.com/Garfield-yin/wework-chat-node/issues/5
[#6]: https://github.com/Garfield-yin/wework-chat-node/issues/6
[#7]: https://github.com/Garfield-yin/wework-chat-node/issues/7
[#8]: https://github.com/Garfield-yin/wework-chat-node/issues/8
