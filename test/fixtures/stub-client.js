/*
 * 配合 LD_PRELOAD 桩 SDK 使用：按 CLIENT_PARAMS 重复调用 getChatData，
 * 桩 SDK 会在进程退出时把参数记录和 slice 计数写进 STUB_REPORT。
 */
const { WeWorkChat } = require("../../index.js");

const w = new WeWorkChat({
	corpid: "x",
	secret: "y",
	private_key: "z",
	seq: 0,
});

const params = JSON.parse(process.env.CLIENT_PARAMS || "{}");
const times = Number(process.env.CLIENT_TIMES || 1);

for (let i = 0; i < times; i++) {
	try {
		w.getChatData(params);
	} catch (err) {
		/* 错误分支正是被测对象，吞掉即可 */
	}
}
