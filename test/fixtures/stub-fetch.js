/*
 * 配合 LD_PRELOAD 桩 SDK 使用：跑一段时间的后台轮询再 stopFetch，
 * 用于验证 fetchData 轮询循环在错误分支上不泄漏 Slice_t。
 */
const { WeWorkChat } = require("../../index.js");

const w = new WeWorkChat({
	corpid: "x",
	secret: "y",
	private_key: "z",
	seq: 0,
});

const runMs = Number(process.env.FETCH_MS || 2000);

w.fetchData(() => {});
setTimeout(() => {
	const seq = w.stopFetch();
	console.log("STOPPED seq=" + seq);
}, runMs);
