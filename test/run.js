#!/usr/bin/env node
/*
 * 测试入口：
 *   1. 检查 addon 是否已编译；
 *   2. 在 Linux 上编译 LD_PRELOAD 桩 SDK（编译不了就跳过依赖它的用例）；
 *   3. 交给 node 内置测试运行器执行 test/*.test.js。
 */
const { spawnSync } = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");
const { buildStub } = require("./helpers");

const ROOT = path.join(__dirname, "..");

function fail(msg) {
	console.error(`\n[test] ${msg}\n`);
	process.exit(1);
}

// 1. addon 必须先编译出来
const addonPath = path.join(ROOT, "build", "Release", "wework.node");
if (!fs.existsSync(addonPath)) {
	if (process.platform !== "linux") {
		console.error(
			"\n[test] 未找到已编译的 addon，且当前平台不是 Linux。\n" +
				"[test] 企业微信只提供 Linux 版 .so，请在 Linux 或 Docker 中运行：\n" +
				"[test]   docker run --rm -v \"$PWD\":/src -w /src node:24 sh -c 'npm install && npm test'\n"
		);
		process.exit(1);
	}
	fail("未找到 build/Release/wework.node，请先执行 `npm run build`。");
}

// 2. 桩 SDK（仅 Linux；其它平台由 native.test.js 自行跳过）
if (process.platform === "linux") {
	if (buildStub() === null) {
		console.warn("[test] 警告：桩 SDK 编译失败，依赖它的用例会失败。请确认已安装 gcc。");
	} else {
		console.log("[test] 桩 SDK 编译完成");
	}
}

// 3. 跑测试（显式列出文件，避免不同 node 版本对目录参数的处理差异）
const testFiles = fs
	.readdirSync(__dirname)
	.filter((f) => f.endsWith(".test.js"))
	.sort()
	.map((f) => path.join("test", f));

if (testFiles.length === 0) {
	fail("test/ 下没有找到 *.test.js");
}

const res = spawnSync(process.execPath, ["--test", ...testFiles], {
	cwd: ROOT,
	stdio: "inherit",
});
process.exit(res.status === null ? 1 : res.status);
