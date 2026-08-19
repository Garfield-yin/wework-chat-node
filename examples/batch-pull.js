/*
 * 增量批量拉取。
 *
 * 重点：last_seq 在本批为空时是 0，不要无条件赋值回 seq，
 * 否则下一轮会从头重新拉取。
 */
const { WeWorkChat } = require("..");
const loadConfig = require("./config");

const wework = new WeWorkChat(loadConfig());

let seq = 0; // 生产环境应从数据库 / Redis 读取上次的进度
let total = 0;

try {
	while (true) {
		const ret = wework.getChatData({ max_results: 1000, timeout: 30, seq });

		if (!ret.data.length) {
			console.log("没有更多数据了");
			break;
		}

		for (const msg of ret.data) {
			// 解密失败的消息会留空（通常是私钥版本不匹配），跳过
			if (!msg) continue;

			const item = JSON.parse(msg);
			console.log(item.msgid, item.msgtype, item.from);
			total++;
		}

		seq = ret.last_seq; // 只在本批有数据时推进
		// 生产环境应在这里持久化 seq，重启后从这里继续
	}
} finally {
	// stopFetch 也负责释放底层 sdk，即使没用过 fetchData 也应该调用
	wework.stopFetch();
}

console.log(`共处理 ${total} 条消息，最终 seq = ${seq}`);
