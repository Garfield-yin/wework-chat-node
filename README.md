# wework-chat-node

使用 node-addon-api 封装了企业微信会话存档金融版 SDK 接口，提供给 node.js 直接调用。

[企业微信获取会话内容文档链接]https://work.weixin.qq.com/api/doc/90000/90135/91774

最近在 docker 环境 [node 24.13.1-slim](node:24.13.1-slim) 做了测试。

### Installation

```
npm install wework-chat-node
```

如果需要升级企业微信 SDK,请按架构更新 `lib/x86/libWeWorkFinanceSdk_C.so`（或 `lib/arm/`）以及对应的
`include/wework/x86/WeWorkFinanceSdk_C.h`（或 `include/wework/arm/`），文件更新后再 build。
本模块也会持续更新优化。

##### Compiling

企业微信只提供了 Linux 版的 `.so`，所以本模块只能在 Linux 下运行。`npm install` 里的编译步骤已经做了平台判断，
在 macOS / Windows 上会跳过 `node-gyp rebuild`，安装本身不会失败，但也无法调用。本地开发建议直接用 Docker
（README 顶部的 node slim 镜像即可）。

### Example

```javascript
import fs from "fs";
import {
	GetMediaDataParams,
	GetDataParams,
	WeWorkChat,
	ChatDataItem,
} from "wework-chat-node";

const privateKey =
	"-----BEGIN RSA PRIVATE KEY-----\n" +
	"xxxxxxxxxxxxxxxxxxxxxxxxxxxx\n" +
	"-----END RSA PRIVATE KEY-----\n";

const wework = new WeWorkChat({
	/** 企业ID */
	corpid: "corpid",
	/** Secret */
	secret: "secret",
	/**私钥，用于消息解密 */
	private_key: privateKey,
	/** 数据拉取index */
	seq: 0,
});

const getMediaData = (
	fileName: string,
	params: GetMediaDataParams,
	bufs: Buffer[] = []
) => {
	const resp = wework.getMediaData(params);
	const bufVal = Buffer.from(resp.data);
	bufs.push(bufVal);
	if (!resp.is_finished) {
		// 分片读写,为了防止大文件 buffer 撑爆，建议使用 stream append 方式写文件
		params.index_buf = resp.buf_index;
		getMediaData(fileName, params, bufs);
	} else {
		const bufVal = Buffer.concat(bufs);

		fs.createWriteStream(fileName).write(bufVal);
	}
};
const test = () => {
	const params: GetDataParams = {
		max_results: 10,
		timeout: 30,
		seq: 1,
	};
	const ret = wework.getChatData(params);
	console.log(ret.last_seq);
	for (const msg of ret.data) {
		if (!msg) continue;
		const msgData: ChatDataItem = JSON.parse(msg);
		if (msgData.msgtype != "file") continue;
		if (msgData.file && msgData.file.fileext != "pptx") continue;
		const fileInfo = msgData.file;
		if (!fileInfo) continue;
		getMediaData(fileInfo.filename, {
			sdk_fileid: fileInfo.sdkfileid,
			index_buf: "",
		});
	}
};

test();
```

### 常见问题

##### Segmentation fault (core dumped)

已知会导致 core dump 的几种情况都已在 v1.2.1 修掉，如果仍然遇到，请先确认升级到最新版本：

- `stopFetch()` 之前会在 800ms 后就 `DestroySdk`，而一次 `GetChatData` 最长要等 30s，
  后台线程会继续使用已经被释放的 sdk。现在 `stopFetch()` 会等后台线程真正退出再释放。
- 重复调用 `stopFetch()` 会把 sdk double free，现在只会释放一次。
- 服务端返回的 JSON 缺字段（例如没有 `chatdata`）时，rapidjson 在 Release 构建下不做成员检查，
  会直接野指针解引用。现在所有字段读取都走存在性判断。
- 参数类型不对（比如 `getChatData()` 不传对象）时，抛出的 JS 异常并不会中断 C++ 执行流，
  下一行就会把非对象当对象用。现在这些分支都会立即返回。
- `new WeWorkChat()` 初始化失败后继续调用其它方法，现在会抛出明确的错误而不是崩溃。

##### 怎么拿到最终的 seq

两种方式：

- `stopFetch()` 的返回值。它会阻塞等待后台线程退出（最长 45s）后再返回，
  所以拿到的是本次 fetch 真正处理完的最后一个 seq。
- `getChatData()` 返回的 `last_seq`，即本批最后一条消息的 seq。
  注意本批为空时 `last_seq` 是 `0`，轮询时请自行保留上一次的 seq，不要无条件赋值回去：

```javascript
let seq = 0;
while (true) {
	const ret = wework.getChatData({ max_results: 1000, timeout: 30, seq });
	if (!ret.data.length) break;
	seq = ret.last_seq; // 只在有数据时推进
	// ... 处理 ret.data
}
```

生产环境建议每处理完一条就把 seq 落库/写 Redis，重启后从落库的 seq 继续；
或者允许少量重复，接收端按 `msgid` 去重。

##### 消息解密失败 / 拿到的 data 里有空值

解密失败最常见的原因是私钥版本对不上。企业微信允许存在多个版本的密钥，每条消息的
`publickey_ver` 指明它该用哪个版本的私钥解密。现在解密失败时会打印该条消息的 `seq` 和
`publickey_ver`，按提示换成对应版本的私钥即可。解密失败的消息在 `data` 里会留空，遍历时请跳过。

### TO DO
