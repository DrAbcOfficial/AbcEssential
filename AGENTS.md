# AGENTS.md

Guidance for AI/automation collaborators working in this repository. Read `README.md` first; it is the only document distributed with the release.

## Hard rules

- Nothing committed (code, comments, docs, commit messages, tags) may contain local machine paths, personal directories, session logs or any credentials; the game directory is always provided through the `ABC_GAME_DIR` environment variable.
- Do not commit test scaffolding, acceptance scripts, evidence screenshots, plans or progress logs; those are session artifacts that do not belong in the release.
- The README is the single public document; interface, command or cvar behaviour changes must update it in the same commit.
- Submodule pins may only point at commits that are already public upstream; confirm the commit is pushed before bumping a pin.
- AbcCore is managed by the aggregate `ABCEnchance` repository and consumed through `ABC_CORE_DIR`; do not add a `thirdparty/AbcCore` submodule back to AbcEssential.
- Keep the plugin free of rendering dependencies: no IMetaRenderer and no hard dependency on Renderer.dll.

## Build

Win32 x86 only:

```bash
cmake --preset win32-debug
cmake --build --preset win32-debug    # Release: win32-release
```

## Conventions

- Naming follows the LLVM style; feature modules implement `IFeatureModule` and self-register, and must not rewrite the chain fields of `hooks::Hooks` directly.
- When touching the export-table chain or sig-scan, preserve the ownership order "this plugin hooks first, AbcUi hooks second" as documented in `src/hooks.h`.
- Commit messages use `type(scope): summary` (feat / fix / refactor / docs / build / chore) with motivation and verification in the body.
- Before handing off: no local paths or credentials; README matches actual behaviour; Debug and Release both build.
