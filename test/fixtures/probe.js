/*
 * 崩溃场景探针：每个场景由父进程单独 fork 一个子进程执行，
 * 父进程通过退出码判断是否收到信号（native 崩溃无法被 JS 的 try/catch 捕获）。
 */
const { WeWorkChat } = require("../../index.js");

const CREDS = {
	corpid: "wwstubcorpid",
	secret: "stubsecret",
	private_key: "not-a-real-key",
	seq: 0,
};

const mk = () => new WeWorkChat(CREDS);

const cases = {
	"ctor-noargs": () => new WeWorkChat(),
	"ctor-empty": () => new WeWorkChat({}),
	"ctor-missing-secret": () => new WeWorkChat({ corpid: "x", private_key: "z" }),
	"ctor-wrong-types": () => new WeWorkChat({ corpid: 1, secret: 2, private_key: 3 }),
	"ctor-ok": () => { mk(); return "constructed"; },

	"getChatData-noargs": () => mk().getChatData(),
	"getChatData-string": () => mk().getChatData("nope"),
	"getChatData-null": () => mk().getChatData(null),

	"getMediaData-noargs": () => mk().getMediaData(),
	"getMediaData-nofileid": () => mk().getMediaData({}),

	"fetchData-noargs": () => mk().fetchData(),
	"fetchData-notfn": () => mk().fetchData(123),

	"stopFetch-twice": () => { const w = mk(); w.stopFetch(); w.stopFetch(); return "survived"; },
	"stopFetch-thrice": () => { const w = mk(); w.stopFetch(); w.stopFetch(); w.stopFetch(); return "survived"; },
	"use-after-stop": () => { const w = mk(); w.stopFetch(); return w.getChatData({ seq: 0 }); },
	"fetch-after-stop": () => { const w = mk(); w.stopFetch(); return w.fetchData(() => {}); },
};

const name = process.argv[2];
if (!cases[name]) {
	console.log("UNKNOWN_CASE");
	process.exit(99);
}

try {
	const result = cases[name]();
	console.log("RETURNED " + JSON.stringify(result === undefined ? null : result));
} catch (err) {
	console.log("THREW " + err.message.split("\n")[0]);
}
