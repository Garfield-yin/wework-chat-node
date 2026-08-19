# wework-chat-node

[![CI](https://github.com/Garfield-yin/wework-chat-node/actions/workflows/ci.yml/badge.svg)](https://github.com/Garfield-yin/wework-chat-node/actions/workflows/ci.yml)
[![npm version](https://img.shields.io/npm/v/wework-chat-node.svg)](https://www.npmjs.com/package/wework-chat-node)
[![node](https://img.shields.io/node/v/wework-chat-node.svg)](https://www.npmjs.com/package/wework-chat-node)
[![license](https://img.shields.io/npm/l/wework-chat-node.svg)](LICENSE)

A Node.js native addon ([node-addon-api](https://github.com/nodejs/node-addon-api))
wrapping WeCom's (WeChat Work / 企业微信) **Chat Archive** SDK, so Node can fetch
and decrypt archived conversations directly.

[中文](README.md) · [API reference (Chinese)](docs/API.md) · [Examples](examples/) · [FAQ (Chinese)](docs/FAQ.md) · [Changelog (Chinese)](CHANGELOG.md)

> Most documentation is in Chinese, since WeCom's Chat Archive is only available
> to mainland-China registered companies. This page covers the essentials in English.

---

## Features

- Fetches and decrypts archived messages (RSA + SDK decryption in one step)
- Two modes: one-shot batch pull (`getChatData`) and a background polling loop (`fetchData`)
- Chunked download of media files (image / voice / video / file)
- Ships TypeScript definitions
- Bundles both x86_64 and arm64 SDK binaries

## Requirements

> [!IMPORTANT]
> **Linux only.** WeCom ships the Chat Archive SDK as a Linux `.so` and nothing
> else, so this module cannot run on macOS or Windows. `npm install` skips the
> compile step on non-Linux platforms — installation succeeds, but the module is
> unusable. Use Docker for local development.

| | |
| ---- | ---- |
| OS | Linux (x86_64 / arm64) |
| Node.js | >= 18; CI covers 18 / 20 / 22 / 24 |
| Build tools | `python3`, `make`, `g++` (compiled at install time) |

## Installation

```bash
npm install wework-chat-node
```

On Debian / Ubuntu, if build tools are missing:

```bash
sudo apt-get install -y python3 build-essential
```

## Quick start

```javascript
const { WeWorkChat } = require("wework-chat-node");

const wework = new WeWorkChat({
	corpid: process.env.WEWORK_CORPID,
	secret: process.env.WEWORK_SECRET,
	// PEM format including header/footer lines. Never hard-code this.
	private_key: process.env.WEWORK_PRIVATE_KEY,
	seq: 0,
});

const ret = wework.getChatData({ max_results: 1000, timeout: 30, seq: 0 });

for (const msg of ret.data) {
	if (!msg) continue; // messages that failed to decrypt are left empty
	const item = JSON.parse(msg);
	console.log(item.msgid, item.msgtype, item.from);
}

console.log("last seq in this batch:", ret.last_seq);
wework.stopFetch(); // releases the SDK
```

### Continuous polling

```javascript
wework.fetchData((msg) => {
	const item = JSON.parse(msg);
	console.log(item.msgid, item.msgtype);
});

process.on("SIGTERM", () => {
	const finalSeq = wework.stopFetch(); // blocks until the worker thread exits
	saveSeq(finalSeq);
});
```

### Incremental polling

`last_seq` is `0` when a batch comes back empty — do not assign it back
unconditionally, or you will restart from the beginning:

```javascript
let seq = await loadSeq();

while (true) {
	const ret = wework.getChatData({ max_results: 1000, timeout: 30, seq });
	if (!ret.data.length) break;

	for (const msg of ret.data) {
		if (!msg) continue;
		await handle(JSON.parse(msg));
	}

	seq = ret.last_seq; // only advance when the batch had data
	await saveSeq(seq); // persist, so a restart resumes here
}
```

## API overview

| Method | Description |
| ------ | ----------- |
| `new WeWorkChat(options)` | Create an instance and initialise the SDK |
| `getChatData(params)` | Synchronously fetch and decrypt one batch |
| `fetchData(callback)` | Start a background polling thread, one callback per message |
| `stopFetch()` | Stop polling, release the SDK, return the final seq (blocking) |
| `getMediaData(params[, cb])` | Fetch a media file, chunked |

### `new WeWorkChat(options)`

| Field | Type | Required | Description |
| ----- | ---- | -------- | ----------- |
| `corpid` | `string` | yes | Corp ID |
| `secret` | `string` | yes | Chat Archive secret |
| `private_key` | `string` | yes | RSA private key (PEM) used to decrypt messages |
| `seq` | `number` | no | Starting position, default `0` |

### `getChatData(params)`

| Field | Type | Required | Default | Description |
| ----- | ---- | -------- | ------- | ----------- |
| `max_results` | `number` | no | `1000` | Batch size. WeCom caps this at 1000; out-of-range values fall back to 1000 |
| `timeout` | `number` | no | `30` | Timeout in seconds |
| `seq` | `number` | no | `0` | Fetch messages **after** this seq |

Returns `{ last_seq: number, data: (string | undefined)[] }`. Each non-empty
entry is a JSON string; `JSON.parse` it to get a message object (see
`ChatDataItem` in `index.d.ts`).

Two gotchas: `last_seq` is `0` for an empty batch, and `data` may contain empty
slots for messages that failed to decrypt (the index still lines up with the
original batch).

### `stopFetch()`

Blocking. Waits for the background thread to actually exit (up to 45s) before
releasing the SDK, so the returned seq is final and the worker never touches
freed memory. Safe to call more than once, but the instance cannot be used
afterwards.

Full details (in Chinese) in [docs/API.md](docs/API.md). Runnable examples live in [examples/](examples/).

## Troubleshooting

Common problems — crashes, build failures, `max_results` being ignored,
messages that will not decrypt — are covered in [docs/FAQ.md](docs/FAQ.md)
(Chinese). A few highlights in English:

- **`Segmentation fault (core dumped)`** — upgrade to v1.2.1 or later. Earlier
  versions crashed when the server returned `errcode:0` with no `chatdata`
  member (the normal response once you are caught up), on a second `stopFetch()`
  call (double free), and on several argument-validation paths.
- **Messages fail to decrypt** — almost always a private key version mismatch.
  Each message carries a `publickey_ver` telling you which key version to use.
  Since v1.2.1 the failure log prints the message's `seq` and `publickey_ver`.
- **Memory grows without bound** — `Slice_t` leaks on every error path, fixed in
  v1.2.1.

## Security

- Never commit private keys or secrets. Read them from environment variables or
  a secret manager.
- Decrypted messages are plaintext chat logs — treat storage, transport and
  backups accordingly.
- This module never logs the private key; it prints only `seq` and
  `publickey_ver` on decryption failure.

See [SECURITY.md](SECURITY.md) to report a vulnerability.

## Development

```bash
npm install          # install deps and compile the addon (Linux only)
npm run build        # recompile
npm test             # run tests (no credentials, no network)
npm run test:docker  # run inside Docker — use this on macOS / Windows
```

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE) © Garfield Yin
