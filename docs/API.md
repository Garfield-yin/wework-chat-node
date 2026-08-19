# API 参考

完整的 TypeScript 类型定义见仓库根目录的 [`index.d.ts`](../index.d.ts)。

接口语义、错误码、私钥与 `publickey_ver` 的定义以
[企业微信官方文档](https://developer.work.weixin.qq.com/document/path/91774)为准。

---

## `new WeWorkChat(options)`

创建实例并初始化底层 SDK。初始化失败会抛出异常。

```javascript
const { WeWorkChat } = require("wework-chat-node");

const wework = new WeWorkChat({
	corpid: process.env.WEWORK_CORPID,
	secret: process.env.WEWORK_SECRET,
	private_key: process.env.WEWORK_PRIVATE_KEY,
	seq: 0,
});
```

### 参数

| 字段 | 类型 | 必填 | 说明 |
| ---- | ---- | ---- | ---- |
| `corpid` | `string` | 是 | 企业 ID |
| `secret` | `string` | 是 | 会话内容存档的 Secret |
| `private_key` | `string` | 是 | 消息解密用的 RSA 私钥（PEM 格式，含首尾行） |
| `seq` | `number` | 否 | 拉取起始位置，默认 `0` |

`private_key` 的格式：

```javascript
const privateKey =
	"-----BEGIN RSA PRIVATE KEY-----\n" +
	"MIIEowIBAAKCAQEA...\n" +
	"-----END RSA PRIVATE KEY-----\n";
```

> **不要把私钥硬编码在代码里**，请从环境变量或密钥管理服务读取。

### 异常

- `Expected one object argument` —— 没传参数或参数不是对象
- `Missing or invalid option: <字段名>` —— 缺少必填字段，或字段不是字符串
- `Init WeWorkFinance sdk error.` —— SDK 初始化失败（通常是 corpid / secret 不对）

---

## `getChatData(params)`

同步拉取一批会话记录并解密。

```javascript
const ret = wework.getChatData({ max_results: 1000, timeout: 30, seq: 0 });
console.log(ret.last_seq);

for (const msg of ret.data) {
	if (!msg) continue; // 解密失败的消息会留空
	const item = JSON.parse(msg);
	console.log(item.msgtype, item.from);
}
```

### 参数

| 字段 | 类型 | 必填 | 默认值 | 说明 |
| ---- | ---- | ---- | ------ | ---- |
| `max_results` | `number` | 否 | `1000` | 单次返回条数。企业微信上限为 1000，超出或非法值按 1000 处理 |
| `timeout` | `number` | 否 | `30` | 超时时间，单位秒 |
| `seq` | `number` | 否 | `0` | 从该 seq **之后**开始拉取 |

> `max_results` 在 v1.2.1 之前不生效（恒为 1000），详见 [#5]。

### 返回值

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| `last_seq` | `number` | 本批最后一条数据的 seq；**本批为空时是 `0`** |
| `data` | `(string \| undefined)[]` | 解密后的消息 JSON 字符串数组 |

`data` 中的每一项用 `JSON.parse` 可得到一条消息对象，其结构见 `index.d.ts`
里的 `ChatDataItem`。

**两个需要注意的地方：**

1. **`last_seq` 在空批次时是 `0`。** 轮询时不要无条件把它赋值回 `seq`，
   否则会从头重新拉取：

   ```javascript
   let seq = 0;
   while (true) {
   	const ret = wework.getChatData({ max_results: 1000, timeout: 30, seq });
   	if (!ret.data.length) break;
   	seq = ret.last_seq; // 只在有数据时推进
   	// ... 处理 ret.data
   }
   ```

2. **`data` 里可能有空值。** 私钥版本不匹配等原因导致解密失败的消息会留空，
   但仍然占据数组位置（保持与原始批次的下标对应）。遍历时请跳过。

### 异常

- `Expected one object argument` —— 参数不是对象
- `WEWORK_CHAT_NODE::GetChatData err ret:<码>` —— SDK 调用失败，错误码含义见官方文档
- `parse json data error` —— 服务端返回的不是合法 JSON
- `get chat message error, errcode:<码> errmsg:<信息>` —— 服务端返回了业务错误
- `WEWORK_CHAT_NODE::sdk unavailable: ...` —— 初始化失败或已经调用过 `stopFetch()`

---

## `fetchData(callback)`

启动后台线程持续轮询，每解密出一条消息就回调一次。返回一个 Promise，
在 `stopFetch()` 之后 resolve。

```javascript
wework.fetchData((msg) => {
	const item = JSON.parse(msg);
	console.log(item.msgid, item.msgtype);
});

process.on("SIGTERM", () => {
	const finalSeq = wework.stopFetch();
	saveSeq(finalSeq);
});
```

### 参数

| 参数 | 类型 | 说明 |
| ---- | ---- | ---- |
| `callback` | `(msg: string) => void` | 每条消息回调一次，参数是解密后的 JSON 字符串 |

### 说明

- 轮询间隔约 150ms，每条消息之间间隔约 80ms（企业微信有频率限制）。
- 单次拉取固定使用上限 1000 条。
- **同一实例同时只能有一个 `fetchData` 在运行**，重复调用会抛异常。
  需要重启拉取时，先 `stopFetch()`。
- 内部会自动推进 `seq`，可通过 `stopFetch()` 的返回值拿到最终值。

### 异常

- `Expected a callback function` —— 没传回调或回调不是函数
- `WEWORK_CHAT_NODE::fetchData is already running, call stopFetch() first`
- `WEWORK_CHAT_NODE::sdk unavailable: ...`

---

## `stopFetch()`

停止后台轮询并释放底层 SDK，返回停止时已拉取到的最后一个 seq。

```javascript
const finalSeq = wework.stopFetch();
```

### 返回值

`number` —— 本次拉取真正处理完的最后一个 seq。

### 说明

- **这是一个阻塞调用。** 它会等待后台线程真正退出（最长 45 秒）后再返回，
  这样才能保证返回的 seq 是最终值，也才能安全地释放 SDK。
  在 v1.2.1 之前它只 sleep 800ms 就返回并释放 SDK，既拿不到最终 seq，
  也会让后台线程访问已释放的内存而崩溃（[#1]、[#6]）。
- **调用之后该实例不能再发起任何请求**，SDK 已经被释放。再调用会抛
  `sdk unavailable`。
- 重复调用是安全的（不会 double free），但只有第一次会真正释放。
- 即使没有调用过 `fetchData`，也可以调用它来释放 SDK。

> 注意：进程被 `kill -9` 时 `stopFetch()` 不会执行。生产环境不要依赖它来
> 持久化 seq，应该每处理完一条就落库，见 [FAQ](FAQ.md)。

---

## `getMediaData(params[, callback])`

拉取媒体文件（图片、语音、视频、文件等）。大文件需要分片多次拉取。

```javascript
const fs = require("fs");

function download(fileName, params) {
	const stream = fs.createWriteStream(fileName);
	let indexBuf = "";
	while (true) {
		const resp = wework.getMediaData({ ...params, index_buf: indexBuf });
		stream.write(Buffer.from(resp.data));
		if (resp.is_finished) break;
		indexBuf = resp.buf_index;
	}
	stream.end();
}
```

### 参数

| 字段 | 类型 | 必填 | 说明 |
| ---- | ---- | ---- | ---- |
| `sdk_fileid` | `string` | 是 | 媒体资源 id，来自消息体里的 `sdkfileid` |
| `index_buf` | `string` | 否 | 分片索引，首次不传，后续传上一次返回的 `buf_index` |

第二个参数可选，传入回调函数则改为回调形式：
成功回调 `(null, resp)` 并返回 `null`，失败回调 `(errMsg)` 并返回 `-1`。

### 返回值

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| `is_finished` | `boolean` | 是否已拉取完整个文件 |
| `buf_index` | `string` | 下一次分片拉取要传的索引；已完成时为空字符串 |
| `data` | `ArrayBuffer` | 本片数据，用 `Buffer.from(resp.data)` 转成 Buffer |

> 大文件建议用 stream 逐片追加写入，不要把所有分片 buffer 攒在内存里再 concat。

### 异常

- `Expected one object argument`
- `Missing or invalid option: sdk_fileid`
- `Get media data error,errorcode:<码>`

---

## 错误码

`10001`~`10003` 属于可重试错误，本模块内部已经做了最多 3 次、间隔 1 秒的重试。
其余错误码含义见
[官方文档](https://developer.work.weixin.qq.com/document/path/91774)，
几个常见的：

| 错误码 | 含义 |
| ------ | ---- |
| `10000` | 参数错误 |
| `10001` | 网络请求错误（自动重试） |
| `10002` | 数据解析失败（自动重试） |
| `10003` | 系统失败（自动重试） |
| `10004` | 密钥错误导致解密失败 |
| `10005` | fileid 错误 |
| `10006` | 解密失败 |
| `10007` | 找不到消息加密版本的私钥，需要重新指定私钥 |
| `10008` | 解析 encrypt_key 失败 |
| `10009` | IP 非法 |
| `10010` | 数据过期 |
| `10011` | 证书错误 |

[#1]: https://github.com/Garfield-yin/wework-chat-node/issues/1
[#5]: https://github.com/Garfield-yin/wework-chat-node/issues/5
[#6]: https://github.com/Garfield-yin/wework-chat-node/issues/6
