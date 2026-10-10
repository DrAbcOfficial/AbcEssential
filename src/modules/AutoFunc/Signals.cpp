// Event-script signals: UserMsg hooks (WeaponList/CurWeapon/Damage/Battery/
// Health/FlashBat), the weapon-ownership diff and the `events` command.
// Split out of AutoFunc.cpp in the Phase 2 audit (old build kept the signal
// sources in core/events, the consumer in autofunc). The old add_weapon/
// drop_weapon source was the full WeaponsResource (now AbcUi's data layer);
// ownership is diffed straight from the prediction weapon_data array, names
// resolved from WeaponList messages.

#include "Signals.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <string>

#include <metahook.h>

#include <weaponinfo.h>

#include <abc/CvarCmd.h>
#include <abc/Console.h>
#include <abc/NetMsg.h>

#include "../../services/Config.h"

namespace Signals {

namespace {

constexpr int kMaxWeapons = 64; // weapon_data_t weapondata[64] (entity_state.h)

constexpr std::array<const char*, 7> s_aryEvents = {
	EVENT_DEATH,
	EVENT_DAMAGE,
	EVENT_HEALTH,
	EVENT_BATTERY,
	EVENT_FLASHBATTERY,
	EVENT_ADDWEAPON,
	EVENT_DROPWEAPON
};

std::map<std::string, std::string> s_dicEventCmd;
std::array<std::string, kMaxWeapons> s_aryWeaponNames;
std::array<bool, kMaxWeapons> s_aryWeaponOwned = {};

// literal placeholder replace - the keys are fixed text, no regex needed
void ReplacePlaceholder(std::string& text, const char* key, const char* value) {
	if (!value)
		return;
	const size_t keyLength = strlen(key);
	size_t pos = 0;
	while ((pos = text.find(key, pos)) != std::string::npos) {
		text.replace(pos, keyLength, value);
		pos += strlen(value);
	}
}

#pragma region UserMsg signal hooks
#define DEFINE_SIGNAL_HOOK(name) \
	static pfnUserMsgHook m_pfn##name; \
	static int __MsgFunc_##name(const char* pszName, int iSize, void* pbuf) \
	/**/
#define CHAIN_ORIGINAL(name) m_pfn##name(pszName, iSize, pbuf)

DEFINE_SIGNAL_HOOK(WeaponList)
{
	abc::MsgReader msg(pbuf, iSize);
	std::string name = msg.readString();
	msg.readChar();  // iAmmoType
	msg.readLong();  // iMax1
	msg.readChar();  // iAmmo2Type
	msg.readLong();  // iMax2
	msg.readChar();  // iSlot
	msg.readChar();  // iSlotPos
	int iId = msg.readShort();
	// iFlags omitted: we only need id -> name
	if (!msg.ok() || iId <= 0 || iId >= kMaxWeapons)
		return CHAIN_ORIGINAL(WeaponList);
	s_aryWeaponNames[iId] = name;
	return CHAIN_ORIGINAL(WeaponList);
}

DEFINE_SIGNAL_HOOK(CurWeapon)
{
	abc::MsgReader msg(pbuf, iSize);
	int iState = msg.readByte();
	int iId = msg.readShort();
	msg.readLong();  // iClip
	msg.readLong();  // iClip2
	if (!msg.ok())
		return CHAIN_ORIGINAL(CurWeapon);
	if (iState <= 0 && iId <= 0)
		TriggerEvent(EVENT_DEATH);
	return CHAIN_ORIGINAL(CurWeapon);
}

DEFINE_SIGNAL_HOOK(Damage)
{
	abc::MsgReader msg(pbuf, iSize);
	int armor = msg.readByte();
	int damageTaken = msg.readByte();
	int tiles = msg.readLong();
	if (!msg.ok() || armor + damageTaken + tiles == 0)
		return CHAIN_ORIGINAL(Damage);
	msg.readCoord();  // vecFrom x/y/z, not consumed by any script
	msg.readCoord();
	msg.readCoord();
	if (!msg.ok())
		return CHAIN_ORIGINAL(Damage);
	TriggerEvent(EVENT_DAMAGE,
		std::to_string(damageTaken).c_str(), std::to_string(armor).c_str(), std::to_string(tiles).c_str());
	return CHAIN_ORIGINAL(Damage);
}

DEFINE_SIGNAL_HOOK(Battery)
{
	abc::MsgReader msg(pbuf, iSize);
	int battery = msg.readShort();
	if (!msg.ok())
		return CHAIN_ORIGINAL(Battery);
	TriggerEvent(EVENT_BATTERY, std::to_string(battery).c_str());
	return CHAIN_ORIGINAL(Battery);
}

DEFINE_SIGNAL_HOOK(Health)
{
	abc::MsgReader msg(pbuf, iSize);
	int health = msg.readLong();
	if (!msg.ok())
		return CHAIN_ORIGINAL(Health);
	TriggerEvent(EVENT_HEALTH, std::to_string(health).c_str());
	return CHAIN_ORIGINAL(Health);
}

DEFINE_SIGNAL_HOOK(FlashBat)
{
	abc::MsgReader msg(pbuf, iSize);
	int flash = msg.readByte();
	if (!msg.ok())
		return CHAIN_ORIGINAL(FlashBat);
	TriggerEvent(EVENT_FLASHBATTERY, std::to_string(flash).c_str());
	return CHAIN_ORIGINAL(FlashBat);
}

void HookMessages() {
	// HOOK_MESSAGE(x) -> g_pMetaHookAPI->HookUserMsg(#x, __MsgFunc_x)
	m_pfnWeaponList = HOOK_MESSAGE(WeaponList);
	m_pfnCurWeapon = HOOK_MESSAGE(CurWeapon);
	m_pfnDamage = HOOK_MESSAGE(Damage);
	m_pfnBattery = HOOK_MESSAGE(Battery);
	m_pfnHealth = HOOK_MESSAGE(Health);
	m_pfnFlashBat = HOOK_MESSAGE(FlashBat);
}
#pragma endregion

}

void Init() {
	// events command
	abc::ADD_COMMAND("events", []() {
		static auto error = []() {
			abc::ConsoleWriteline("events [operate] [event] [cmd]\n");
			abc::ConsoleWriteline("operate: 0 = delete, 1 = set\n");
			abc::ConsoleWriteline("aviliable events:\n");
			for (const char* name : s_aryEvents) {
				std::string ss = "    [";
				ss += name;
				ss += "]\n";
				abc::ConsoleWriteline(ss.c_str());
			}
		};
		size_t argc = gEngfuncs.Cmd_Argc();
		if (argc < 3) {
			error();
			return;
		}
		const char* evt = gEngfuncs.Cmd_Argv(2);
		if (std::find(s_aryEvents.begin(), s_aryEvents.end(), evt) == s_aryEvents.end()) {
			error();
			return;
		}
		switch (std::atoi(gEngfuncs.Cmd_Argv(1))) {
		case 0:
			s_dicEventCmd.erase(evt);
			break;
		case 1: {
			if (argc < 4) {
				error();
				return;
			}
			s_dicEventCmd[evt] = gEngfuncs.Cmd_Argv(3);
			break;
		}
		default:error();break;
		}
	});
	// replay persisted event bindings
	s_dicEventCmd = abcess::Config()->m_dicEventsCmd;

	HookMessages();
}

void Exit() {
	abcess::Config()->m_dicEventsCmd = s_dicEventCmd;
}

void ResetWeapons() {
	s_aryWeaponNames.fill({});
	s_aryWeaponOwned.fill(false);
}

void OnTxferPredictionData(const weapon_data_s* wd) {
	for (int i = 1; i < kMaxWeapons; i++) {
		bool owned = wd[i].m_iId > 0;
		if (owned == s_aryWeaponOwned[i])
			continue;
		s_aryWeaponOwned[i] = owned;
		// scripts key on the weapon name; unknown ids (no WeaponList yet) are skipped
		if (s_aryWeaponNames[i].empty())
			continue;
		TriggerEvent(owned ? EVENT_ADDWEAPON : EVENT_DROPWEAPON, s_aryWeaponNames[i].c_str());
	}
}

void TriggerEvent(const char* eventname,
	const char* param1, const char* param2, const char* param3, const char* param4) {
	auto found = s_dicEventCmd.find(eventname);
	if (found == s_dicEventCmd.end() || found->second.empty())
		return;
	std::string ssbuf = found->second;
	ReplacePlaceholder(ssbuf, "{param1}", param1);
	ReplacePlaceholder(ssbuf, "{param2}", param2);
	ReplacePlaceholder(ssbuf, "{param3}", param3);
	ReplacePlaceholder(ssbuf, "{param4}", param4);
	abc::EngineClientCmd(ssbuf.c_str());
}

}
