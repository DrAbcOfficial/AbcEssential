#pragma once

struct usercmd_s;

namespace autofunc {

// Registers cl_autojump, +ducktap/-ducktap and the concurrent command chain
// once per process, then replays persisted concurrent bindings on every
// HUD_Init (after the base HUD has registered its own commands). The replay
// must run per cycle because HUD_Shutdown destroys the chained command
// objects. Event scripts and their signal feeds live in signals.h.
void init();
// Writes concurrent bindings back into the plugin config (HUD_Shutdown).
void shutdown();

void autoJump(struct usercmd_s *cmd);
void duckTap(struct usercmd_s *cmd);

// Movement-state feed wired by src/hooks.cpp (HUD_PlayerMove).
void onPlayerMove(bool inWater, bool onGround, bool walking);

} // namespace autofunc
