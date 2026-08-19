# 更新日志

本文件记录本项目的所有重要变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

## [1.3.0] - 2026-08-19

本次没有改动运行时的 C++ 代码，主要是工程化与文档，外加一次依赖大版本升级。

### Added

- GitHub Actions CI：在 node 18 / 20 / 22 / 24 × x64 / arm64 共 8 个组合上
  构建并运行测试，另有独立任务验证 `npm pack` 产物可安装可用。
- 测试套件（`npm test`），共 30 个用例，不需要企业微信凭据也不联网：
  - 崩溃回归测试，每个场景独立子进程运行并检查退出信号；
  - 基于 `LD_PRELOAD` 桩 SDK 的参数透传与 `Slice_t` 泄漏断言。
  - 这些用例在 v1.2.1 之前的代码上有 19 个失败。
- 补齐开源项目文档：`LICENSE`、`CONTRIBUTING.md`、`CODE_OF_CONDUCT.md`、
  `SECURITY.md`、`CHANGELOG.md`、`docs/API.md`、`docs/FAQ.md`、
  issue / PR 模板、Dependabot 配置。
- 新增 `examples/`：增量批量拉取、后台轮询与优雅停止、媒体文件分片下载。
  凭据统一从环境变量读取。

### Changed

- README 重写，补充完整的 API 说明、运行环境要求与故障排查入口。
- `package.json` 元信息补全：`types`、`bugs`、`homepage`、`keywords`。
- **不再声明支持 Node 16**，`engines` 调整为 `>=18`。此前声明的
  `~10 >=10.20 || >=12.17` 并不准确；升级到 node-addon-api 8 后最低要求为
  Node 18（其 `engines` 为 `^18 || ^20 || >= 21`）。Node 16 已于 2023-09 EOL。
- 依赖升级：node-addon-api 7.1.1 → 8.9.2，编译无告警，测试全部通过。
- `.gitignore` 整理，明确忽略可能包含真实凭据的本地调试文件。

## [1.2.1] - 2026-08-19

修复了一批会导致进程崩溃和内存泄漏的问题。**强烈建议从 1.2.0 及更早版本升级。**

### Fixed

- **`getChatData` 的 `max_results` 参数完全没有生效**（[#5]）。参数被读进了一个
  同名遮蔽的无用字符串，实际调用 SDK 时用的一直是常量 `1000`。现在会正确透传，
  并限制在 `1~1000`。`timeout` / `seq` 也改为按缺省值处理 —— 之前对 `undefined`
  调 `ToNumber()` 得到 `NaN` 再转 `int64_t` 属于未定义行为。
- **`getChatData` 的三个错误分支都会泄漏 `Slice_t`**（[#7]）。
- **`fetchData` 轮询循环的两个 `continue` 分支都会泄漏 `Slice_t`**（[#8]）。
  实测每秒泄漏约 7 个且无上限。现已改为 RAII 管理，所有出口都会释放。
- **多处 `Segmentation fault (core dumped)`**（[#6]）：
  - `stopFetch()` 只 sleep 800ms 就 `DestroySdk`，而一次 `GetChatData` 最长
    30s、一批 1000 条消息分发要 80s，后台线程会继续使用已释放的 sdk。
    现在会等线程真正退出再释放；
  - 重复调用 `stopFetch()` 会 double free；
  - 服务端返回 `errcode:0` 但没有 `chatdata` 字段时（拉到最新的正常响应），
    rapidjson 在 Release 构建下不做成员检查，直接野指针解引用；
  - 参数校验失败后没有 `return`，而 `NAPI_DISABLE_CPP_EXCEPTIONS` 下抛出
    JS 异常并不会中断 C++ 执行流；
  - `initSdk` 失败后留下半初始化的 sdk 指针；
  - 后台线程直接持有 `this`，JS 对象被 GC 回收后就是 use-after-free。
- 重复调用 `fetchData` 会起多个线程同时改写 `seq_`，现在会明确报错。
- `getMediaData` 的可选参数 `index_buf` 不传时会得到字面量 `"undefined"`
  并被当作真实的分片索引传给 SDK。
- `MsgData` 每条消息 `new` 之后从未 `delete`。
- `rsa_pri_decrypt` 在提前返回的分支上没有释放 BIO。

### Added

- 解密失败时输出该条消息的 `seq` 和 `publickey_ver`，便于定位私钥版本不匹配
  的问题（此前是静默跳过）。思路来自 [#3]，但不会输出私钥。
- `package.json` 增加 `files` 白名单，避免把无关文件打进发布产物。

### Changed

- `binding.gyp` 显式固定 `OPENSSL_API_COMPAT=0x10100000L`，修复 node 17+
  内置 OpenSSL 3.0 引发的编译报错（[#4]），同时消除了全部 `RSA_*` 弃用告警。
- `stopFetch()` 现在会阻塞等待后台线程退出（最长 45s）后再返回，
  因此返回的 seq 是本次拉取真正的最终值（[#1]）。
- `sprintf` 改为 `snprintf`。
- `index.d.ts` 修正：`getChatData` 的参数改为可选并注明默认值，
  说明 `last_seq` 在空批次时为 `0`，`data` 数组可能含空值。

## [1.2.0] - 2026-03-11

### Changed

- 升级企业微信会话存档 SDK 版本。
- 升级 Node 版本支持，在 `node:24-slim` 环境下验证。

## [1.1.2] - 2022-05-28

### Added

- 增加更多消息类型支持。
- `getChatData` 返回结果新增 `last_seq`（本批最后一条数据的 seq）。

## [1.1.1] - 2022-05-26

### Fixed

- 修正 `index.d.ts` 类型定义。

## [1.1.0] - 2022-05-20

### Added

- 支持 TypeScript 类型定义。

### Changed

- 企业微信支持了会话存档消息推送，因此不再强制使用循环拉取方式，
  可以直接调用 `getChatData` 获取数据。

## [1.0.0] - 2020-12-18

### Fixed

- 修复多个 app 接入时数据串号的问题。

## 更早版本

- 2020-10-22 对 `10001`~`10003` 错误码增加重试策略；修复获取媒体文件数据的错误。
- 2020-10-20 项目初始化。

[Unreleased]: https://github.com/Garfield-yin/wework-chat-node/compare/v1.3.0...HEAD
[1.3.0]: https://github.com/Garfield-yin/wework-chat-node/compare/v1.2.1...v1.3.0
[1.2.1]: https://github.com/Garfield-yin/wework-chat-node/compare/1.2.0...v1.2.1
[1.2.0]: https://github.com/Garfield-yin/wework-chat-node/compare/v1.1.2...1.2.0
[1.1.2]: https://github.com/Garfield-yin/wework-chat-node/compare/v1.1.1...v1.1.2
[1.1.1]: https://github.com/Garfield-yin/wework-chat-node/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/Garfield-yin/wework-chat-node/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/Garfield-yin/wework-chat-node/releases/tag/v1.0.0
[#1]: https://github.com/Garfield-yin/wework-chat-node/issues/1
[#3]: https://github.com/Garfield-yin/wework-chat-node/pull/3
[#4]: https://github.com/Garfield-yin/wework-chat-node/issues/4
[#5]: https://github.com/Garfield-yin/wework-chat-node/issues/5
[#6]: https://github.com/Garfield-yin/wework-chat-node/issues/6
[#7]: https://github.com/Garfield-yin/wework-chat-node/issues/7
[#8]: https://github.com/Garfield-yin/wework-chat-node/issues/8
