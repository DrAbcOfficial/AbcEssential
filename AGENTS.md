# AGENTS.md

Guidance for AI/automation collaborators working in this repository. Read `README.md` first; it is the only document distributed with the release.

## Hard rules

- Nothing committed (code, comments, docs, commit messages, tags) may contain local machine paths, personal directories, session logs or any credentials; the deployment destination is provided through the `ABC_GAME_DIR` environment variable (optional; unset means no deploy).
- Do not commit test scaffolding, acceptance scripts, evidence screenshots, plans or progress logs; those are session artifacts that do not belong in the release.
- The README is the single public document; interface, command or cvar behaviour changes must update it in the same commit.
- Submodule pins may only point at commits that are already public upstream; confirm the commit is pushed before bumping a pin.
- AbcCore is managed by the aggregate `ABCEnchance` repository and consumed through `ABC_CORE_DIR`; do not add a `thirdparty/AbcCore` submodule back to AbcEssential.
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

## Lifecycle and runtime contracts

- HUD_Init/HUD_Shutdown repeat within one engine process (the client re-runs
  its HUD cycle on map init). Only cvar/command registration and module
  `onInit` stay process-scoped; `H_HUD_Init` re-arms everything HUD_Shutdown
  released: the client detour (`installClientHooks`, idempotent through the
  owned list plus `FindInlineHook`) and the post-chain replay
  (`moduleRegistry().postInitAll()`, which rebuilds the destroyed concurrent
  command chains). The six usermsg hooks are installed once and re-chained by
  `signals::refreshAfterClientLoad()` only after a client DLL reload detected
  through the client image; re-hooking on every cycle would make this chain
  and AbcUi's point at each other.
- `autofunc::init` and `signals::init` keep their command registration and
  usermsg install behind a one-shot latch but replay persisted bindings on
  every call; `signals::refreshAfterClientLoad` is the only re-hook path and
  it is probe-gated (the helper still ignores its own previous handler). Do
  not add an unconditional `HOOK_MESSAGE` outside that path.
- `CHAIN_ORIGINAL` tolerates a message the client did not register (null
  predecessor); keep that guard when adding signal sources.
- The effect assets under `assets/svencoop/abc/` are runtime dependencies of
  the fx module and are part of both `install()` (deploy) and `dist`; adding a
  new spr/mdl there needs no extra rule, but do not move assets out of that
  tree without updating both packaging paths.
- `events 1 <event> <cmd ...>` joins the remaining console tokens, so scripts
  may contain spaces without quoting; quoted input still works.
- CI pins the CMake download (version + SHA-256) and action SHAs, and does not
  use a personal access token: keep it that way when editing the workflow.
