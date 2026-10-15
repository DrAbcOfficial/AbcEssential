# AGENTS.md

面向在本仓库中工作的 AI/自动化协作者。开始前先读 `README.md`（本仓库唯一随发布分发的说明文档）。

## 硬性约束

- 提交内容（代码、注释、文档、提交信息、标签）不得出现本机路径、个人目录、会话记录或任何凭证；游戏目录一律通过 `ABC_GAME_DIR` 环境变量提供。
- 不提交测试脚手架、验收脚本、证据截图、计划与进度记录等会话产物。
- README 是唯一对外文档；接口、命令或 cvar 行为变化必须同步更新 README。
- 子模块 pin 只指向对应上游已公开的提交；升级 pin 前先确认该提交已推送。
- 保持零渲染依赖：不得引入 IMetaRenderer 或对 Renderer.dll 的硬依赖。

## 构建

仅 Win32 x86：

```bash
cmake --preset win32-debug
cmake --build --preset win32-debug    # Release：win32-release
```

## 约定

- 命名遵循 LLVM 风格；功能模块实现 `IFeatureModule` 并自注册，不直接改写 `hooks::Hooks` 的链上字段。
- 改动导出表链或 sig-scan 时保持「本插件先挂、AbcUi 后挂」的所有权顺序，见 `src/hooks.h`。
- 提交信息使用 `type(scope): 摘要`（feat / fix / refactor / docs / build / chore），正文说明动机与验证方式。
- 交付前自查：无本机路径与凭证；README 与实际行为一致；Debug 与 Release 均可构建。
