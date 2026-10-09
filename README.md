# AbcEssential

Sven Co-op / MetaHookSv 非 UI 实用功能插件（`AbcEssential.dll`）：find / models / autofunc（自动连跳、ducktap、concurrent 命令链、events 事件脚本）/ viewmodellag + 相机 / 特效增强（血液、弹壳、高斯、egon）。共享库 [AbcCore](https://github.com/DrAbcOfficial/AbcCore) 以 submodule 静态链入。

零渲染依赖：不加载 IMetaRenderer，也不要求 Renderer.dll 在场。

## 目录结构

```text
src/
├── plugins.cpp            # 插件入口（CAbcPluginBase 派生 + EXPOSE_SINGLE_INTERFACE）
├── hooks.h/.cpp           # sig-scan、InlineHook 装卸、导出表链式 thunk（链唯一所有者）
├── services/              # 插件级公共设施
│   ├── Config.h/.cpp      #   config schema + 旧版一次性导入
│   ├── ExtraPrecache.h/.cpp
│   └── Version.h
└── modules/               # 功能模块（IFeatureModule 自注册）
    ├── Module.h/.cpp      #   模块接口 + 注册表
    ├── find/ models/ autofunc/ viewmodellag/ camera/ fx/
```

命名遵循 LLVM 风格；模块经注册表接入，不直接改写 `hooks::Hooks` 的链上字段。

## 构建

Windows + MSVC（VS 2026）+ CMake ≥ 4.2，仅 Win32 x86：

```bash
git clone --recursive https://github.com/DrAbcOfficial/AbcEssential.git
cmake --preset win32-release
cmake --build --preset win32-release
```

嵌套子模块未随 `--recursive` 拉取时，在 `thirdparty/AbcCore` 内再执行一次 `git submodule update --init --recursive`。

## 部署

配置时设置环境变量 `ABC_GAME_DIR` 为游戏根目录（或传 `-DABC_GAME_DIR=...`）；未指定时不创建部署目标：

```bash
cmake --build --preset win32-release --target deploy
# 或：cmake --install build/win32-release --config Release
```

DLL 安装到 `<game>/svencoop/metahook/plugins/`；特效资产沿用 `abcenchance/spr|mdl/`（gauss_wave、gauss_loophole、egon_ball）；配置文件为 `svencoop/abc/abcessential/config.json`，首次运行自动从旧 `abcenchance/config.json` 导入（旧文件保留）。

## plugins.lst 顺序

`<game>/svencoop/metahook/configs/plugins.lst` 中保持在 Renderer 之后、AbcUi 之前：

```text
VGUI2Extension.dll
Renderer.dll
AbcEssential.dll
AbcUi.dll
```

链式钩子约定：本插件先挂导出表，AbcUi 后挂（后挂者把先挂者的实现当作原函数链入）；移除本插件无需改动其他插件。

## 命令 / cvar 一览

| 类别 | 名称 | 说明 |
|---|---|---|
| 命令 | `find <关键字>` | 模糊搜索 cvar/命令（受保护 cvar 显示 `**`） |
| 命令 | `models [前缀]` / `models fav\|unfav <名>` | 列出玩家模型目录；收藏持久化（`favorite_models`） |
| 命令 | `concurrent <命令> <附带命令>` / `removeconcurrent <命令>` | 命令链，持久化（`concurrent_cmds`） |
| 命令 | `events <0\|1> <事件> <命令串>` | 事件→命令脚本，持久化（`event_cmds`）；支持 `{param1..4}` |
| 事件 | death / damage / health / battery / flash_battery / add_weapon / drop_weapon | damage 参数：伤害/护甲/命中部位；武器事件参数：武器名 |
| 命令 | `+ducktap` / `-ducktap` | 自动蹲跳 |
| cvar | `cl_autojump` | 自动连跳 |
| cvar | `cl_modellag` / `cl_modellag_autostop` / `cl_modellag_value` | 武器摆动（开镜自动停摆） |
| cvar | `cl_modelslide` / `cl_modelslide_angle` / `cl_modelslide_length` / `cl_modelslide_height` | 武器滑动 |
| cvar | `cam_idealheight` / `cam_idealright` | 第三人称视角偏移 |
| cvar | `abc_bloodefx` / `abc_bloodsprite_speed` / `abc_bloodsprite_num` | 血液增强 |
| cvar | `abc_shellefx` | 弹壳淡出 + 硝烟 |
| cvar | `abc_gaussefx` | 高斯主开火重制（穿透/反射/弹孔） |
| cvar | `abc_egonefx` | egon 粒子球 |
| cvar | `abc_version` | 版本号（只读） |

## 子模块

| 路径 | 上游 | pin |
|---|---|---|
| `thirdparty/AbcCore` | https://github.com/DrAbcOfficial/AbcCore | `9ae4a5d17dd2df110dcbe7fea7156fac89fd54ca` |
