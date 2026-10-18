# AGENTS.md

Guidance for AI/automation collaborators working in this repository. Read `README.md` first; it is the only document distributed with the release.

## Hard rules

- Nothing committed (code, comments, docs, commit messages, tags) may contain local machine paths, personal directories, session logs or any credentials; the deployment destination is provided through the `ABC_GAME_DIR` environment variable (optional; unset means no deploy).
- Do not commit test scaffolding, acceptance scripts, evidence screenshots, plans or progress logs; those are session artifacts that do not belong in the release.
- The README is the single public document; interface, command or cvar behaviour changes must update it in the same commit.
- Submodule pins may only point at commits that are already public upstream; confirm the commit is pushed before bumping a pin.
- Keep the plugin free of rendering dependencies: no IMetaRenderer and no hard dependency on Renderer.dll.

## Build

Win32 x86 only / C++20 / dynamic CRT (/MD). AbcCore is consumed as
`AbcCore.dll` from `ABC_CORE_DIR` (sibling `../AbcCore` by default); there is no
`thirdparty/AbcCore` checkout. `ABC_GAME_DIR` is optional: unset means the
plugin is built but not deployed.

```bash
cmake --preset win32-debug
cmake --build --preset win32-debug    # Release: win32-release
```

## Conventions

- Formatting follows LLVM (2 spaces, 80 columns, attached braces). Identifier
  casing follows the repository conventions: ABCEnchance-derived functions keep
  their UpperCamelCase spelling, new lower-level helpers use lowerCamelCase and
  constants use the `k` prefix.
- C-style casts remain in ABCEnchance-derived code; new code uses named casts.
- The plugin defines the five metahook globals in `src/CoreGlobals.cpp`;
  `CAbcPluginBase` copies them into `AbcCore.dll`. Do not add a second copy in
  another translation unit.
- Feature modules implement `IFeatureModule` and self-register; they must not
  rewrite the chain-owned fields of `hooks::Hooks` (type `hooks::HookedFuncs`)
  directly.
- When touching the export-table chain or gamedata symbol resolution, preserve
  the ownership order "this plugin hooks first, AbcUi hooks second" as
  documented in `src/hooks.h`.
- Commit messages use `type(scope): summary` (feat / fix / refactor / docs / build / chore) with motivation and verification in the body.
- Before handing off: no local paths or credentials; README matches actual behaviour; Debug and Release both build.
