/*
 * 回归测试：参数校验与生命周期管理。
 *
 * 这些场景在 v1.2.1 之前会让进程直接收到 SIGSEGV / SIGABRT（见 issue #6）。
 * 关键在于「不崩溃」，所以每个场景都在独立子进程里跑并检查退出信号 ——
 * native 崩溃是抓不到 JS 异常的。
 */
const test = require("node:test");
const assert = require("node:assert");
const { runProbe } = require("./helpers");

/** 应当抛出 JS 异常而不是崩溃 */
const shouldThrow = [
	["ctor-noargs", "构造函数不传参数"],
	["ctor-empty", "构造函数缺 corpid"],
	["ctor-missing-secret", "构造函数缺 secret"],
	["ctor-wrong-types", "构造函数参数类型错误"],
	["getChatData-noargs", "getChatData 不传参数"],
	["getChatData-string", "getChatData 传字符串"],
	["getChatData-null", "getChatData 传 null"],
	["getMediaData-noargs", "getMediaData 不传参数"],
	["getMediaData-nofileid", "getMediaData 缺 sdk_fileid"],
	["fetchData-noargs", "fetchData 不传回调"],
	["fetchData-notfn", "fetchData 回调不是函数"],
	["use-after-stop", "stopFetch 之后再调用 getChatData"],
	["fetch-after-stop", "stopFetch 之后再调用 fetchData"],
];

for (const [name, desc] of shouldThrow) {
	test(`${desc} 应抛异常而非崩溃 [${name}]`, () => {
		const r = runProbe(name);
		assert.strictEqual(r.crashed, false, `进程崩溃了: signal=${r.signal} status=${r.status}\n${r.stderr}`);
		assert.match(r.stdout, /^THREW /m, `期望抛出异常，实际: ${r.stdout}`);
	});
}

/** 应当正常完成而不崩溃 */
const shouldSurvive = [
	["ctor-ok", "正常构造"],
	["stopFetch-twice", "重复调用 stopFetch（曾经 double free 导致 SIGSEGV）"],
	["stopFetch-thrice", "连续三次调用 stopFetch"],
];

for (const [name, desc] of shouldSurvive) {
	test(`${desc} 不应崩溃 [${name}]`, () => {
		const r = runProbe(name);
		assert.strictEqual(r.crashed, false, `进程崩溃了: signal=${r.signal} status=${r.status}\n${r.stderr}`);
		assert.match(r.stdout, /^RETURNED /m, `期望正常返回，实际: ${r.stdout}`);
	});
}
