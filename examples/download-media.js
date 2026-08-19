/*
 * 分片下载媒体文件。
 *
 * 大文件务必像这样逐片写入，不要把所有分片攒在内存里再 Buffer.concat。
 */
const fs = require("node:fs");
const path = require("node:path");
const { WeWorkChat } = require("..");
const loadConfig = require("./config");

const wework = new WeWorkChat(loadConfig());

function download(sdkFileId, destPath) {
	const stream = fs.createWriteStream(destPath);
	let indexBuf = "";
	let chunks = 0;

	while (true) {
		const resp = wework.getMediaData({ sdk_fileid: sdkFileId, index_buf: indexBuf });
		stream.write(Buffer.from(resp.data));
		chunks++;

		if (resp.is_finished) break;
		indexBuf = resp.buf_index; // 续传索引
	}

	stream.end();
	console.log(`${destPath} 下载完成，共 ${chunks} 个分片`);
}

const outDir = path.join(__dirname, "downloads");
fs.mkdirSync(outDir, { recursive: true });

try {
	const ret = wework.getChatData({ max_results: 100, timeout: 30, seq: 0 });

	for (const msg of ret.data) {
		if (!msg) continue;
		const item = JSON.parse(msg);

		if (item.msgtype === "file" && item.file) {
			download(item.file.sdkfileid, path.join(outDir, item.file.filename));
		} else if (item.msgtype === "image" && item.image) {
			download(item.image.sdkfileid, path.join(outDir, `${item.msgid}.jpg`));
		}
	}
} finally {
	wework.stopFetch();
}
