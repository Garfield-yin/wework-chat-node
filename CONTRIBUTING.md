# 贡献指南

感谢你愿意参与改进这个项目。

## 一件必须先知道的事

企业微信只提供 **Linux** 版的会话存档 SDK（`lib/x86/`、`lib/arm/` 下的 `.so`），
所以本模块**只能在 Linux 上构建和运行**。在 macOS / Windows 上：

- `npm install` 会跳过编译步骤，不会报错，但模块无法使用；
- `npm test` 会提示你去 Linux 或 Docker 里跑。

## 开发环境

### 用 Docker（推荐，macOS / Windows 必须）

```bash
npm run test:docker
```

或者手动进容器折腾：

```bash
docker run --rm -it -v "$PWD":/src -w /w node:24 bash
# 容器内：
cp -r /src/. /w && npm install && npm test
```

### 直接在 Linux 上

需要 `python3`、`make`、`g++`：

```bash
sudo apt-get install -y python3 build-essential   # Debian / Ubuntu

npm install      # 安装依赖并编译 addon
npm run build    # 只重新编译（等价于 node-gyp rebuild）
npm test
```

## 测试

测试不需要企业微信凭据，也不联网。

```bash
npm test
```

测试分三层：

| 文件 | 作用 |
| ---- | ---- |
| `test/smoke.test.js` | 模块能加载、导出符合预期 |
| `test/crash.test.js` | 参数校验与生命周期，**每个场景独立子进程**，检查是否收到信号 |
| `test/native.test.js` | 参数透传与 native 内存管理，依赖 `LD_PRELOAD` 桩 SDK |

### 为什么崩溃测试要开子进程

native 崩溃（SIGSEGV / SIGABRT）不会抛 JS 异常，`try/catch` 抓不到，
整个进程会直接没掉。所以每个场景必须单独 fork 一个进程，靠退出码判断：
退出码 ≥ 128 表示收到了信号（139 = SIGSEGV，134 = SIGABRT）。

### 桩 SDK 是怎么回事

`test/sdk-stub.c` 通过 `LD_PRELOAD` 覆盖真实 `.so` 里的
`NewSlice` / `FreeSlice` / `GetContentFromSlice` / `GetChatData`，从而可以：

- 记录 addon 实际传给 `GetChatData` 的参数 —— 用来断言 `max_results` 之类的参数
  确实透传了（历史上它被静默忽略过，见 issue #5）；
- 统计 `Slice_t` 的分配与释放是否配对 —— 用来断言错误分支没有泄漏
  （见 issue #7、#8）；
- 让"服务端"返回各种异常响应（解析失败、`errcode` 非 0、缺 `chatdata` 字段），
  这些分支在真实环境里很难稳定复现。

`NewSdk` / `Init` / `DestroySdk` 不覆盖，走真实实现。

## 改 C++ 代码时请注意

这几条都是踩过坑的：

1. **编译时定义了 `NAPI_DISABLE_CPP_EXCEPTIONS`**，
   `ThrowAsJavaScriptException()` 只是把异常挂起，**不会中断 C++ 执行流**。
   抛完必须立刻 `return`，否则下一行照样会执行。

2. **rapidjson 的 `operator[]` 在 Release 构建（`NDEBUG`）下不做成员检查**，
   字段不存在就是野指针解引用。读字段请走 `wework.cc` 里的
   `FindMember` / `GetStringMember` / `GetInt64Member`。

3. **`NewSlice()` 必须配对 `FreeSlice()`**。请使用 `SliceGuard`（RAII），
   不要手写 `FreeSlice`；需要转移所有权时用 `SliceGuard::release()`。

4. **后台线程与 sdk 的生命周期**。`stopFetch()` 会等后台线程真正退出后再
   `DestroySdk`，改动 `fetchData` / `EndFetchData` 时请保持这个顺序。

5. **不要把私钥写进日志。** 解密失败时只输出 `seq` 和 `publickey_ver`。

改完之后请补上对应的测试 —— 现有的每一条测试都对应一个真实发生过的 bug。

## 提交 PR

1. 从 `main` 开出分支；
2. 保证 `npm test` 全绿；
3. 涉及 API 改动的，同步更新 `index.d.ts` 和 `docs/API.md`；
4. 在 `CHANGELOG.md` 的 `Unreleased` 段落里加一条；
5. 提交信息建议用
   [Conventional Commits](https://www.conventionalcommits.org/zh-hans/)
   风格（`fix:` / `feat:` / `docs:` / `chore:`）。

CI 会在 node 18 / 20 / 22 / 24 × x64 / arm64 共 8 个组合上构建并跑测试，
另外还会验证 `npm pack` 出来的产物能被正常安装和使用。

## 发布（维护者）

```bash
npm version <patch|minor|major>   # 更新版本号并打 tag
# 更新 CHANGELOG.md
git push && git push --tags
npm publish                       # 需要 npm 2FA
```

升级企业微信 SDK 时，按架构替换 `lib/{x86,arm}/libWeWorkFinanceSdk_C.so`
和对应的 `include/wework/{x86,arm}/WeWorkFinanceSdk_C.h`，然后重新构建并跑测试。
