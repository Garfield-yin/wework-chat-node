## 改动说明

<!-- 这个 PR 做了什么，为什么这么做。如果修复了 issue，写上 Fixes #123 -->

## 改动类型

- [ ] Bug 修复
- [ ] 新功能
- [ ] 文档
- [ ] 构建 / CI
- [ ] 重构（不改变外部行为）

## 验证方式

<!--
本项目是原生模块，只能在 Linux 上构建和运行。请说明你怎么验证的：

    npm run build && npm test

或用 Docker：

    npm run test:docker

改动涉及 C++ 的话，请特别说明是否新增了测试用例。
-->

## 检查项

- [ ] `npm test` 全部通过
- [ ] 涉及 C++ 行为改动的，补充了对应的测试（`test/`）
- [ ] 涉及 API 改动的，同步更新了 `index.d.ts` 和 `docs/API.md`
- [ ] 更新了 `CHANGELOG.md`
- [ ] 没有在代码、日志或示例里泄露 secret / 私钥
