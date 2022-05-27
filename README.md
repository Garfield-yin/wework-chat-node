# wework-chat-node

使用 node-addon-api 封装了企业微信会话存档金融版 SDK 接口，提供给 node.js 直接调用。

[企业微信获取会话内容文档链接]https://work.weixin.qq.com/api/doc/90000/90135/91774

### Installation

```
npm install wework-chat-node
```

如果需要升级企业微信 SDK,请更新 lib/libWeWorkFinanceSdk_C.so 以及 include/wework/WeWorkFinanceSdk_C.h，文件更新后再 build。
本模块也会持续更新优化。

##### Compiling

由于企业微信提供的 sdk 仅支持 linux 与 windows,在 OS X 下可编译成功，但无法正常使用。

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

### TO DO
