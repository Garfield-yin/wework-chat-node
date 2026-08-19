/*
 * 冒烟测试：模块能加载、导出符合预期。
 * 不需要企业微信凭据，也不联网。
 */
const test = require("node:test");
const assert = require("node:assert");

test("模块可以加载并导出 WeWorkChat", () => {
	const addon = require("../index.js");
	assert.strictEqual(typeof addon.WeWorkChat, "function", "应导出 WeWorkChat 构造函数");
});

test("实例应具备全部公开方法", () => {
	const { WeWorkChat } = require("../index.js");
	const w = new WeWorkChat({ corpid: "x", secret: "y", private_key: "z", seq: 0 });
	for (const method of ["getChatData", "getMediaData", "fetchData", "stopFetch"]) {
		assert.strictEqual(typeof w[method], "function", `缺少方法 ${method}`);
	}
	w.stopFetch();
});

test("stopFetch 应返回数字类型的 seq", () => {
	const { WeWorkChat } = require("../index.js");
	const w = new WeWorkChat({ corpid: "x", secret: "y", private_key: "z", seq: 42 });
	const seq = w.stopFetch();
	assert.strictEqual(typeof seq, "number");
	assert.strictEqual(seq, 42, "未开始拉取时应返回初始 seq");
});
