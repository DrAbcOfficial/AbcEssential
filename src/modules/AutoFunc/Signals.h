#pragma once

// Event-script signal layer: the `events` command, the UserMsg feeds and the
// weapon-ownership diff that fire registered scripts (plan 04 §3 - signals
// are parsed in-plugin, not through AbcUi). The command surface for movement
// automation and concurrent chains lives in AutoFunc.h.

struct weapon_data_s;

namespace Signals {

// Event names (04 §3). These are the keys of event_cmds in the plugin config
// - renaming one breaks persisted user scripts.
constexpr char EVENT_DEATH[] = "death";
constexpr char EVENT_DAMAGE[] = "damage";
constexpr char EVENT_HEALTH[] = "health";
constexpr char EVENT_BATTERY[] = "battery";
constexpr char EVENT_FLASHBATTERY[] = "flash_battery";
constexpr char EVENT_ADDWEAPON[] = "add_weapon";
constexpr char EVENT_DROPWEAPON[] = "drop_weapon";

// Registers the `events` command + the UserMsg hooks below and replays
// persisted event bindings (HUD_Init, after the base chain).
void Init();
// Writes event bindings back into the plugin config (HUD_Shutdown).
void Exit();

// Map transition / HUD_VidInit: weapon ownership starts from scratch.
void ResetWeapons();
// HUD_TxferPredictionData feed: fires add_weapon/drop_weapon on ownership
// changes, names resolved from WeaponList messages.
void OnTxferPredictionData(const struct weapon_data_s* wd);

// Replaces {param1}..{param4} in the registered command string and runs it.
void TriggerEvent(const char* eventname,
	const char* param1 = nullptr, const char* param2 = nullptr,
	const char* param3 = nullptr, const char* param4 = nullptr);

}
