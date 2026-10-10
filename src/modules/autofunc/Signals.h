#pragma once

// Event-script signal layer: the `events` command, the UserMsg feeds and the
// weapon-ownership diff that fire registered scripts (plan 04 §3 - signals
// are parsed in-plugin, not through AbcUi). The command surface for movement
// automation and concurrent chains lives in autofunc.h.

struct weapon_data_s;

namespace signals {

// Event names (04 §3). These are the keys of event_cmds in the plugin config
// - renaming one breaks persisted user scripts.
constexpr char kEventDeath[] = "death";
constexpr char kEventDamage[] = "damage";
constexpr char kEventHealth[] = "health";
constexpr char kEventBattery[] = "battery";
constexpr char kEventFlashBattery[] = "flash_battery";
constexpr char kEventAddWeapon[] = "add_weapon";
constexpr char kEventDropWeapon[] = "drop_weapon";

// Registers the `events` command + the UserMsg hooks below and replays
// persisted event bindings (HUD_Init, after the base chain).
void init();
// Writes event bindings back into the plugin config (HUD_Shutdown).
void shutdown();

// Map transition / HUD_VidInit: weapon ownership starts from scratch.
void resetWeapons();
// HUD_TxferPredictionData feed: fires add_weapon/drop_weapon on ownership
// changes, names resolved from WeaponList messages.
void onTxferPredictionData(const struct weapon_data_s* wd);

// Replaces {param1}..{param4} in the registered command string and runs it.
void triggerEvent(const char* eventname,
	const char* param1 = nullptr, const char* param2 = nullptr,
	const char* param3 = nullptr, const char* param4 = nullptr);

}
