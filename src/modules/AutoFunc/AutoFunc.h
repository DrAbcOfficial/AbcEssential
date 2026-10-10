#pragma once

struct usercmd_s;

namespace AutoFunc {

// Registers cl_autojump, +ducktap/-ducktap and the concurrent command chain,
// then replays persisted concurrent bindings (HUD_Init, after the base HUD
// has registered its own commands). Event scripts and their signal feeds
// live in Signals.h.
void Init();
// Writes concurrent bindings back into the plugin config (HUD_Shutdown).
void Exit();

void AutoJump(struct usercmd_s* cmd);
void DuckTap(struct usercmd_s* cmd);

// Movement-state feed wired by src/hooks.cpp (HUD_PlayerMove).
void OnPlayerMove(bool bInWater, bool bOnGround, bool bWalking);

}
