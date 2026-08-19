const { spawnSync } = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

const ROOT = path.join(__dirname, "..");
const STUB_SO = path.join(os.tmpdir(), "wework-sdk-stub.so");

/**
 * 在独立子进程里跑一个探针场景。
 * native 崩溃不会抛 JS 异常，只能靠退出码/信号判断，所以必须开子进程。
 */
function runProbe(caseName) {
	const res = spawnSync(process.execPath, [path.join(__dirname, "fixtures", "probe.js"), caseName], {
		cwd: ROOT,
		encoding: "utf8",
		timeout: 60000,
	});
	return {
		signal: res.signal,
		status: res.status,
		stdout: (res.stdout || "").trim(),
		stderr: (res.stderr || "").trim(),
		crashed: res.signal !== null || (res.status !== null && res.status >= 128),
	};
}

/** 编译 LD_PRELOAD 桩 SDK，返回 .so 路径；编译器不可用时返回 null。 */
function buildStub() {
	const cc = process.env.CC || "cc";
	const res = spawnSync(cc, ["-shared", "-fPIC", "-O1", "-o", STUB_SO, path.join(__dirname, "sdk-stub.c")], {
		encoding: "utf8",
	});
	if (res.status !== 0) {
		return null;
	}
	return STUB_SO;
}

/**
 * 挂上桩 SDK 跑一个 fixture，返回桩写出的报告。
 * 报告含每次 GetChatData 收到的实参，以及 Slice_t 的分配/释放计数。
 */
function runWithStub(fixture, { mode = "ok", env = {}, timeout = 60000 } = {}) {
	const report = path.join(os.tmpdir(), `wework-stub-report-${process.pid}-${Math.random().toString(36).slice(2)}.json`);
	const res = spawnSync(process.execPath, [path.join(__dirname, "fixtures", fixture)], {
		cwd: ROOT,
		encoding: "utf8",
		timeout,
		env: {
			...process.env,
			...env,
			LD_PRELOAD: STUB_SO,
			STUB_MODE: mode,
			STUB_REPORT: report,
		},
	});

	let parsed = null;
	if (fs.existsSync(report)) {
		try {
			parsed = JSON.parse(fs.readFileSync(report, "utf8"));
		} catch {
			parsed = null;
		}
		fs.unlinkSync(report);
	}

	return {
		signal: res.signal,
		status: res.status,
		stdout: (res.stdout || "").trim(),
		stderr: (res.stderr || "").trim(),
		crashed: res.signal !== null || (res.status !== null && res.status >= 128),
		report: parsed,
	};
}

module.exports = { ROOT, STUB_SO, runProbe, buildStub, runWithStub };
