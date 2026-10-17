# AbcEssential

Non-UI utility plugin for Sven Co-op / MetaHookSv (`AbcEssential.dll`): find / models / autofunc (autojump, ducktap, concurrent command chains, event scripts) / viewmodel lag + camera / effect enhancements (blood, shells, gauss, egon). The shared [AbcCore](https://github.com/DrAbcOfficial/AbcCore) library is linked statically as a submodule.

Zero rendering dependencies: no IMetaRenderer is loaded and Renderer.dll is not required to be present.

## Layout

```text
src/
├── plugins.cpp            # entry point (CAbcPluginBase + EXPOSE_SINGLE_INTERFACE)
├── hooks.h/.cpp           # sig-scan, inline-hook install/remove, chained export thunks (single owner of the chain)
├── services/              # plugin-level services
│   ├── Config.h/.cpp      #   config schema + one-shot legacy import
│   ├── ExtraPrecache.h/.cpp
│   └── Version.h
└── modules/               # feature modules (self-registered through IFeatureModule)
    ├── Module.h/.cpp      #   module interface + registry
    ├── find/ models/ autofunc/ viewmodellag/ camera/ fx/
```

Naming follows the LLVM style; modules plug in through the registry and never rewrite the chain fields of `hooks::Hooks` directly.

## Build

Windows + MSVC + CMake >= 4.2, Win32 x86 only:

```bash
git clone --recursive https://github.com/DrAbcOfficial/AbcEssential.git
cmake --preset win32-release
cmake --build --preset win32-release
```

If the nested submodules were not fetched by `--recursive`, run `git submodule update --init --recursive` inside `thirdparty/AbcCore` once.

## Deploy

Set the `ABC_GAME_DIR` environment variable to the game root before configuring (or pass `-DABC_GAME_DIR=...`); nothing is deployed when it is unset:

```bash
cmake --build --preset win32-release --target deploy
# or: cmake --install build/win32-release --config Release
```

The DLL installs to `<game>/svencoop/metahook/plugins/`; effect assets keep the legacy `abcenchance/spr|mdl/` layout (gauss_wave, gauss_loophole, egon_ball); the config file is `svencoop/abc/abcessential/config.json`, imported once from the legacy `abcenchance/config.json` on first run (the old file is kept).

## Packaging

```bash
cmake --build --preset win32-release --target dist
```

Produces `AbcEssential-<version>.zip` under `build/<preset>/dist/`, laid out to unpack into the Sven Co-op root (`svencoop/metahook/plugins/AbcEssential.dll`, generated gamedata, README).

## plugins.lst order

Keep it after Renderer and before AbcUi in `<game>/svencoop/metahook/configs/plugins.lst`:

```text
VGUI2Extension.dll
Renderer.dll
AbcEssential.dll
AbcUi.dll
```

Chained-hook contract: this plugin hooks the export table first and AbcUi hooks afterwards (the later hook chains the earlier implementation in as its original); removing this plugin requires no changes to the others.

## Commands / cvars

| Kind | Name | Description |
|---|---|---|
| command | `find <keyword>` | Fuzzy-search cvars and console commands (protected cvars print `**`) |
| command | `models [prefix]` / `models fav\|unfav <name>` | List player model directories; favorites persist (`favorite_models`) |
| command | `concurrent <command> <extra command>` / `removeconcurrent <command>` | Command chains, persisted (`concurrent_cmds`) |
| command | `events <0\|1> <event> <command string>` | Event-to-command scripts, persisted (`event_cmds`); `{param1..4}` supported |
| events | death / damage / health / battery / flash_battery / add_weapon / drop_weapon | damage parameters: damage / armor / hit group; weapon events carry the weapon name |
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

## Submodules

| Path | Upstream | Pin |
|---|---|---|
| `thirdparty/AbcCore` | https://github.com/DrAbcOfficial/AbcCore | `9ae4a5d17dd2df110dcbe7fea7156fac89fd54ca` |

## License

Licensed under the [BSD 3-Clause License](LICENSE); each dependency keeps its own license.
