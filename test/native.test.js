/*
 * 回归测试：参数透传与 native 内存管理。
 *
 * 通过 LD_PRELOAD 挂一个桩 SDK（test/sdk-stub.c），既能拿到 addon 实际传给
 * GetChatData 的实参，也能统计 NewSlice / FreeSlice 是否配对。
 * 仅 Linux 可用，其它平台整体跳过。
 */
const test = require("node:test");
const assert = require("node:assert");
const { runWithStub } = require("./helpers");

const skip = process.platform !== "linux" ? "LD_PRELOAD 桩 SDK 仅支持 Linux" : false;

test("max_results 应透传给 SDK（issue #5）", { skip }, () => {
	const r = runWithStub("stub-client.js", {
		mode: "ok",
		env: { CLIENT_PARAMS: JSON.stringify({ max_results: 7, seq: 3, timeout: 11 }), CLIENT_TIMES: "1" },
	});
	assert.strictEqual(r.crashed, false, `进程崩溃: signal=${r.signal}`);
	assert.ok(r.report, "桩 SDK 未写出报告");
	assert.strictEqual(r.report.calls.length, 1);
	assert.strictEqual(r.report.calls[0].limit, 7, "max_results 没有透传（修复前恒为 1000）");
	assert.strictEqual(r.report.calls[0].seq, 3, "seq 没有透传");
	assert.strictEqual(r.report.calls[0].timeout, 11, "timeout 没有透传");
});

test("max_results 超过 1000 应被钳制到 1000", { skip }, () => {
	const r = runWithStub("stub-client.js", {
		mode: "ok",
		env: { CLIENT_PARAMS: JSON.stringify({ max_results: 5000, seq: 0, timeout: 30 }), CLIENT_TIMES: "1" },
	});
	assert.ok(r.report);
	assert.strictEqual(r.report.calls[0].limit, 1000, "企业微信上限是 1000，超出应钳制");
});

test("省略可选参数时应使用默认值", { skip }, () => {
	const r = runWithStub("stub-client.js", {
		mode: "ok",
		env: { CLIENT_PARAMS: JSON.stringify({}), CLIENT_TIMES: "1" },
	});
	assert.ok(r.report);
	assert.strictEqual(r.report.calls[0].limit, 1000);
	assert.strictEqual(r.report.calls[0].seq, 0);
	assert.strictEqual(r.report.calls[0].timeout, 30);
});

/* getChatData 的每个错误分支都曾经泄漏一个 Slice_t（issue #7） */
for (const mode of ["ok", "ret_err", "retry_err", "bad_json", "errcode", "no_chatdata"]) {
	test(`getChatData 在 mode=${mode} 下不应泄漏 Slice_t（issue #7）`, { skip }, () => {
		const r = runWithStub("stub-client.js", {
			mode,
			env: { CLIENT_PARAMS: JSON.stringify({ max_results: 10, seq: 0, timeout: 5 }), CLIENT_TIMES: "5" },
		});
		assert.strictEqual(r.crashed, false, `进程崩溃: signal=${r.signal}\n${r.stderr}`);
		assert.ok(r.report, "桩 SDK 未写出报告");
		assert.ok(r.report.alloc > 0, "没有分配过 Slice_t，测试本身可能失效了");
		assert.strictEqual(
			r.report.leaked,
			0,
			`泄漏了 ${r.report.leaked} 个 Slice_t (alloc=${r.report.alloc} free=${r.report.free})`
		);
	});
}

/* fetchData 轮询循环的两个 continue 分支曾经每次轮询泄漏一个 Slice_t（issue #8） */
for (const mode of ["ret_err", "errcode"]) {
	test(`fetchData 轮询在 mode=${mode} 下不应泄漏 Slice_t（issue #8）`, { skip, timeout: 60000 }, () => {
		const r = runWithStub("stub-fetch.js", { mode, env: { FETCH_MS: "2000" } });
		assert.strictEqual(r.crashed, false, `进程崩溃: signal=${r.signal}\n${r.stderr}`);
		assert.match(r.stdout, /^STOPPED seq=\d+$/m, `fetchData 没有正常停止: ${r.stdout}`);
		assert.ok(r.report, "桩 SDK 未写出报告");
		assert.ok(r.report.callCount > 1, `轮询次数太少 (${r.report.callCount})，测试可能失效`);
		assert.strictEqual(
			r.report.leaked,
			0,
			`泄漏了 ${r.report.leaked} 个 Slice_t (alloc=${r.report.alloc} free=${r.report.free})`
		);
	});
}
