# AbcEssential

Non-UI utility plugin for Sven Co-op / MetaHookSv (`AbcEssential.dll`): find / models / autofunc (autojump, ducktap, concurrent command chains, event scripts) / viewmodel lag + camera / effect enhancements (blood, shells, gauss, egon). The shared [AbcCore](https://github.com/DrAbcOfficial/AbcCore) library is consumed as `AbcCore.dll` from the aggregate/`ABC_CORE_DIR` checkout; no AbcCore copy lives under `thirdparty/`.

Zero rendering dependencies: no IMetaRenderer is loaded and Renderer.dll is not required to be present.

## Layout

```text
src/
├── plugins.cpp            # entry point (CAbcPluginBase + EXPOSE_SINGLE_INTERFACE)
├── CoreGlobals.cpp        # per-plugin metahook globals copied into AbcCore.dll
├── hooks.h/.cpp           # gamedata symbol resolution, inline-hook install/remove, chained export thunks (single owner of the chain)
├── services/              # plugin-level services
│   ├── Config.h/.cpp      #   config schema + one-shot legacy import
│   ├── ExtraPrecache.h/.cpp
│   └── Version.h
└── modules/               # feature modules (self-registered through IFeatureModule)
    ├── Module.h/.cpp      #   module interface + registry
    ├── find/ models/ autofunc/ viewmodellag/ camera/ fx/
```

Formatting follows LLVM; modules plug in through the registry and never rewrite the chain-owned fields of `hooks::Hooks` (`hooks::HookedFuncs`) directly.

## Build

Windows + MSVC + CMake >= 4.2, Win32 x86 only, C++20, dynamic CRT (/MD):

```bash
git clone --recursive https://github.com/DrAbcOfficial/AbcEssential.git
git clone --recursive https://github.com/DrAbcOfficial/AbcCore.git ../AbcCore
cmake --preset win32-release
cmake --build --preset win32-release
```

The default `ABC_CORE_DIR` is the sibling `../AbcCore` checkout; pass
`-DABC_CORE_DIR=<path>` when AbcCore lives elsewhere. The build produces
`AbcEssential.dll` against `AbcCore.lib`; deploy `AbcCore.dll` alongside the
plugins (the aggregate package puts it under `metahook/dlls/AbcCore/`).

## Deploy

Set the `ABC_GAME_DIR` environment variable to the game root before configuring (or pass `-DABC_GAME_DIR=...`); nothing is deployed when it is unset:

```bash
cmake --build --preset win32-release --target deploy
# or: cmake --install build/win32-release --config Release
```

The DLL installs to `<game>/svencoop/metahook/plugins/`. AbcEssential does not ship effect assets: the FX code precaches the legacy `abcenchance/spr|mdl/` paths (gauss_wave, gauss_loophole, egon_ball) from the game's existing content. The config file is `svencoop/abc/abcessential/config.json`, imported once from the legacy `abcenchance/config.json` on first run (the old file is kept).

## Packaging

```bash
cmake --build --preset win32-release --target dist
```

Produces `AbcEssential-<version>.zip` under `build/<preset>/dist/`. The archive
root is `AbcEssential-<version>/`, containing `svencoop/` (plugin DLL and
generated gamedata) and `README.md`; copy the versioned `svencoop/` directory
into the Sven Co-op root (or extract while stripping the version folder).
`AbcCore.dll` is provided by the aggregate package, not this zip.

## plugins.lst order

Keep it after Renderer and before AbcUi in `<game>/svencoop/metahook/configs/plugins.lst`. Renderer is not a runtime dependency of AbcEssential; the order is the standard MetaHookSv plugin order so the chained export table stays consistent.

```text
VGUI2Extension.dll
Renderer.dll
AbcEssential.dll
AbcUi.dll
```

Chained-hook contract: this plugin hooks the export table first and AbcUi hooks afterwards (the later hook chains the earlier implementation in as its original); removing this plugin requires no changes to the others. `AbcCore.dll` is a shared dependency, not a plugin in this list; place it under `svencoop/metahook/dlls/AbcCore/`.

## Commands / cvars

| Kind | Name | Description |
|---|---|---|
| command | `find <keyword>` | Fuzzy-search cvars and console commands (protected cvars print `**`) |
| command | `models [prefix]` / `models fav\|unfav <name>` | List player model directories; favorites persist (`favorite_models`) |
| command | `concurrent <command> <extra command>` / `removeconcurrent <command>` | Command chains, persisted (`concurrent_cmds`) |
| command | `events <0\|1> <event> <command string>` | Event-to-command scripts, persisted (`event_cmds`); `{param1..4}` supported |
| events | death / damage / health / battery / flash_battery / add_weapon / drop_weapon | damage parameters: damage / armor / tiles; weapon events carry the weapon name |
| command | `+ducktap` / `-ducktap` | Automatic crouch-jump |
| cvar | `cl_autojump` | Automatic bunny-hop |
| cvar | `cl_modellag` / `cl_modellag_autostop` / `cl_modellag_value` | Weapon sway (stops while scoped) |
| cvar | `cl_modelslide` / `cl_modelslide_angle` / `cl_modelslide_length` / `cl_modelslide_height` | Weapon slide |
| cvar | `cam_idealheight` / `cam_idealright` | Third-person view offset |
| cvar | `abc_bloodefx` / `abc_bloodsprite_speed` / `abc_bloodsprite_num` | Blood enhancement |
| cvar | `abc_shellefx` | Shell fade-out + smoke |
| cvar | `abc_gaussefx` | Gauss primary-fire rework (penetration / reflection / decals) |
| cvar | `abc_egonefx` | Egon particle ball |
| cvar | `abc_version` | Version string (read-only) |

## License

Licensed under the [BSD 3-Clause License](LICENSE); each dependency keeps its own license.
