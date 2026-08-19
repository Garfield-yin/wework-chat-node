/*
 * 后台持续轮询，以及如何优雅停止。
 *
 * stopFetch() 会阻塞等待后台线程真正退出（最长 45s）再返回，
 * 所以它的返回值就是本次拉取真正处理完的最后一个 seq。
 */
const { WeWorkChat } = require("..");
const loadConfig = require("./config");

const wework = new WeWorkChat(loadConfig());

let count = 0;

wework
	.fetchData((msg) => {
		const item = JSON.parse(msg);
		count++;
		console.log(`[${count}] ${item.msgid} ${item.msgtype} from=${item.from}`);

		// 生产环境建议每处理完一条就持久化 item.seq，
		// 进程被 kill -9 时 stopFetch() 根本不会执行
	})
	.then(() => console.log("后台线程已退出"));

const shutdown = () => {
	console.log("\n正在停止…");
	const finalSeq = wework.stopFetch();
	console.log(`最终 seq = ${finalSeq}，共收到 ${count} 条消息`);
	process.exit(0);
};

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);

console.log("开始拉取，Ctrl+C 停止…");
