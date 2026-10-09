#pragma once

struct usercmd_s;
struct weapon_data_s;

namespace AutoFunc {

// Event-script signal names (04 §3).
constexpr char EVENTCMD_DEATH[] = "death";
constexpr char EVENTCMD_DAMAGE[] = "damage";
constexpr char EVENTCMD_HEALTH[] = "health";
constexpr char EVENTCMD_BATTERY[] = "battery";
constexpr char EVENTCMD_FLASHBATTERY[] = "flash_battery";
constexpr char EVENTCMD_ADDWEAPON[] = "add_weapon";
constexpr char EVENTCMD_DROPWEAPON[] = "drop_weapon";

// Registers cl_autojump, +ducktap/-ducktap, concurrent/removeconcurrent and
// events; replays persisted concurrent/event bindings; hooks the minimal
// usermsg set feeding the signals below (HUD_Init time, after the base chain).
void Init();
// Writes concurrent/event bindings back into the plugin config (HUD_Shutdown).
void Exit();

void AutoJump(struct usercmd_s* cmd);
void DuckTap(struct usercmd_s* cmd);

// Signal feeds wired by src/hooks.cpp.
void OnPlayerMove(bool bInWater, bool bOnGround, bool bWalking);
void OnTxferPredictionData(const struct weapon_data_s* wd);
// Map transition: weapon ownership starts from scratch.
void ResetWeapons();

// Replaces {param1}..{param4} in the registered command string and runs it.
void TriggerEvent(const char* eventname,
	const char* param1 = nullptr, const char* param2 = nullptr,
	const char* param3 = nullptr, const char* param4 = nullptr);

}
