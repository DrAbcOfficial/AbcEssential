# AbcEssential

Utility plugin for Sven Co-op: console search, player model management, movement helpers, viewmodel and camera tweaks, and effect enhancements. It requires [AbcCore](https://github.com/DrAbcOfficial/AbcCore), which is included in the [ABCEnchance](https://github.com/DrAbcOfficial/ABCEnchance) package.

## Features

- **Find** — search console commands and cvars by keyword.
- **Models** — list the installed player models and mark favorites.
- **Autofunc** — autojump, ducktap and command chains.
- **Events** — run your own commands when something happens in the game.
- **Viewmodel** — weapon sway and weapon slide.
- **Camera** — third-person view offset.
- **Effects** — blood, shell casings, gauss and egon.

## Install

Extract the archive into the Sven Co-op folder. It contains `svencoop` at its root, so there is no extra folder to copy.

Then add `AbcEssential.dll` to `svencoop/metahook/configs/plugins.lst` in this order:

```text
VGUI2Extension.dll
Renderer.dll
AbcEssential.dll
AbcUi.dll
```

AbcCore.dll is required and must be installed under `svencoop/metahook/dlls/AbcCore/`. It is not listed in `plugins.lst`.

## Commands and cvars

| Name | Description |
|---|---|
| `find <keyword>` | Search console commands and cvars. |
| `models [prefix]` | List player models, optionally filtered by prefix. |
| `models fav <name>` / `models unfav <name>` | Add or remove a favorite player model. |
| `concurrent <command> <extra command>` | Run an extra command together with a command. |
| `removeconcurrent <command>` | Remove a command chain. |
| `events <0\|1> <event> <command>` | Run a command when an event happens. |
| `+ducktap` / `-ducktap` | Automatic crouch-jump. |
| `cl_autojump` | Automatic bunny-hop. |
| `cl_modellag` / `cl_modellag_autostop` / `cl_modellag_value` | Weapon sway. |
| `cl_modelslide` / `cl_modelslide_angle` / `cl_modelslide_length` / `cl_modelslide_height` | Weapon slide. |
| `cam_idealheight` / `cam_idealright` | Third-person view offset. |
| `abc_bloodefx` / `abc_bloodsprite_num` / `abc_bloodsprite_speed` | Blood effects. |
| `abc_shellefx` | Shell casing effects. |
| `abc_gaussefx` | Gauss effect rework. |
| `abc_egonefx` | Egon particle ball. |
| `abc_version` | Show the plugin version. |

Supported events for `events`: `death`, `damage`, `health`, `battery`, `flash_battery`, `add_weapon` and `drop_weapon`.

## Build the dist package

Requires Windows, Visual Studio 2026 with the C++ workload, CMake 4.2 or newer, and the AbcCore source next to AbcEssential.

```bash
git clone https://github.com/DrAbcOfficial/AbcEssential.git
git clone --recursive https://github.com/DrAbcOfficial/AbcCore.git ../AbcCore
cd AbcEssential
cmake --preset win32-release
cmake --build --preset win32-release --target dist
```

The archive is written to `build/win32-release/dist/AbcEssential-<version>.zip`.

If AbcCore is checked out elsewhere, pass `-DABC_CORE_DIR=<path>` when configuring.

## License

Licensed under the [BSD 3-Clause License](LICENSE).
