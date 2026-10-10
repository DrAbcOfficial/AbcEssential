// AutoFunc: autojump / ducktap / concurrent command chains / event scripts.
// Ported from ABCEnchance src/core/module/autofunc.* (GBK -> UTF-8).
//
// Signal sources are self-contained here (plan 04 §3): minimal usermsg hooks
// (WeaponList/CurWeapon/Damage/Battery/Health/FlashBat) plus the
// HUD_PlayerMove and HUD_TxferPredictionData feeds wired through src/hooks.cpp.
// The old build derived add_weapon/drop_weapon from its full WeaponsResource
// (now AbcUi's data layer); here ownership is diffed straight from the
// prediction weapon_data array, names resolved from WeaponList messages.
#include "AutoFunc.h"

#include <array>
#include <algorithm>
#include <cstring>
#include <map>
#include <regex>
#include <string>

#include <metahook.h>

#include <usercmd.h>
#include <weaponinfo.h>
#include <in_buttons.h>

#include <abc/CvarCmd.h>
#include <abc/Console.h>
#include <abc/Events.h>

#include "MsgReader.h"
#include "../../Config.h"
#include "../Module.h"

namespace {

constexpr int kMaxWeapons = 64; // weapon_data_t weapondata[64] (entity_state.h)

cvar_t* s_pCvarAutoJump = nullptr;
bool s_bAutoDucktap = false;

using concurrentcmd_t = struct {
	std::string cmd;
	std::string concurrent;
	xcommand_t origincall;
};
std::map<std::string, concurrentcmd_t*> s_dicConcurrentCmds;
std::map<std::string, std::string> s_dicEventCmd;

const std::array<std::string, 7> s_aryEvents = {
	AutoFunc::EVENTCMD_DEATH,
	AutoFunc::EVENTCMD_DAMAGE,
	AutoFunc::EVENTCMD_HEALTH,
	AutoFunc::EVENTCMD_BATTERY,
	AutoFunc::EVENTCMD_FLASHBATTERY,
	AutoFunc::EVENTCMD_ADDWEAPON,
	AutoFunc::EVENTCMD_DROPWEAPON
};

using playermove_t = struct {
	bool inwater;
	bool onground;
	bool walking;
};
playermove_t s_playerMove = {};

std::array<std::string, kMaxWeapons> s_aryWeaponNames;
std::array<bool, kMaxWeapons> s_aryWeaponOwned = {};

void SetConcurrent(const char* cmd, const char* concurrent) {
	if (g_pMetaHookAPI->FindCmd(cmd) == nullptr)
		return;

	if (!_stricmp(cmd, "concurrent") || !_stricmp(cmd, "removeconcurrent")) {
		abc::ConsoleWriteline("concurrent and removeconcurrent does not support this!\n");
		return;
	}
	//FIXME: yes, this is stupid, but i dont wanna do a parser for now
	std::string ss = concurrent;
	if (ss.find(cmd) != std::string::npos) {
		abc::ConsoleWriteline("There are recursive calls in concurrent command!\n");
		return;
	}
	if (s_dicConcurrentCmds.find(cmd) == s_dicConcurrentCmds.end()) {
		concurrentcmd_t* concurrentobj = new concurrentcmd_t();
		concurrentobj->cmd = cmd;
		concurrentobj->concurrent = concurrent;
		concurrentobj->origincall = g_pMetaHookAPI->HookCmd(cmd, []() {
			const char* nowcmd = gEngfuncs.Cmd_Argv(0);
			auto finditer = s_dicConcurrentCmds.find(nowcmd);
			if (finditer != s_dicConcurrentCmds.end()) {
				gEngfuncs.pfnClientCmd(finditer->second->concurrent.c_str());
				finditer->second->origincall();
			}
		});
		s_dicConcurrentCmds.emplace(std::make_pair(cmd, concurrentobj));
	}
	else {
		auto obj = s_dicConcurrentCmds[cmd];
		obj->concurrent = concurrent;
	}
}

void RemoveConcurrent(const char* cmd) {
	auto iter = s_dicConcurrentCmds.find(cmd);
	if (iter != s_dicConcurrentCmds.end()) {
		concurrentcmd_t* obj = iter->second;
		g_pMetaHookAPI->HookCmd(obj->cmd.c_str(), obj->origincall);
		delete obj;
		s_dicConcurrentCmds.erase(iter);
	}
	else
		abc::ConsoleWriteline("There is no concurrent in this command!\n");
}

#pragma region UserMsg signal hooks
#define DEFINE_SIGNAL_HOOK(name) \
	static pfnUserMsgHook m_pfn##name; \
	static int __MsgFunc_##name(const char* pszName, int iSize, void* pbuf) \
	/**/
#define CHAIN_ORIGINAL(name) m_pfn##name(pszName, iSize, pbuf)

DEFINE_SIGNAL_HOOK(WeaponList)
{
	CMsgReader msg(pbuf, iSize);
	std::string name = msg.readString();
	msg.readChar();  // iAmmoType
	msg.readLong();  // iMax1
	msg.readChar();  // iAmmo2Type
	msg.readLong();  // iMax2
	msg.readChar();  // iSlot
	msg.readChar();  // iSlotPos
	int iId = msg.readShort();
	// iFlags omitted: we only need id -> name
	if (!msg.readOK() || iId <= 0 || iId >= kMaxWeapons)
		return CHAIN_ORIGINAL(WeaponList);
	s_aryWeaponNames[iId] = name;
	return CHAIN_ORIGINAL(WeaponList);
}

DEFINE_SIGNAL_HOOK(CurWeapon)
{
	CMsgReader msg(pbuf, iSize);
	int iState = msg.readByte();
	int iId = msg.readShort();
	msg.readLong();  // iClip
	msg.readLong();  // iClip2
	if (!msg.readOK())
		return CHAIN_ORIGINAL(CurWeapon);
	if (iState <= 0 && iId <= 0)
		AutoFunc::TriggerEvent(AutoFunc::EVENTCMD_DEATH);
	return CHAIN_ORIGINAL(CurWeapon);
}

DEFINE_SIGNAL_HOOK(Damage)
{
	CMsgReader msg(pbuf, iSize);
	int armor = msg.readByte();
	int damageTaken = msg.readByte();
	int tiles = msg.readLong();
	if (!msg.readOK() || armor + damageTaken + tiles == 0)
		return CHAIN_ORIGINAL(Damage);
	msg.readCoord();  // vecFrom x/y/z, not consumed by any script
	msg.readCoord();
	msg.readCoord();
	if (!msg.readOK())
		return CHAIN_ORIGINAL(Damage);
	AutoFunc::TriggerEvent(AutoFunc::EVENTCMD_DAMAGE,
		std::to_string(damageTaken).c_str(), std::to_string(armor).c_str(), std::to_string(tiles).c_str());
	return CHAIN_ORIGINAL(Damage);
}

DEFINE_SIGNAL_HOOK(Battery)
{
	CMsgReader msg(pbuf, iSize);
	int battery = msg.readShort();
	if (!msg.readOK())
		return CHAIN_ORIGINAL(Battery);
	AutoFunc::TriggerEvent(AutoFunc::EVENTCMD_BATTERY, std::to_string(battery).c_str());
	return CHAIN_ORIGINAL(Battery);
}

DEFINE_SIGNAL_HOOK(Health)
{
	CMsgReader msg(pbuf, iSize);
	int health = msg.readLong();
	if (!msg.readOK())
		return CHAIN_ORIGINAL(Health);
	AutoFunc::TriggerEvent(AutoFunc::EVENTCMD_HEALTH, std::to_string(health).c_str());
	return CHAIN_ORIGINAL(Health);
}

DEFINE_SIGNAL_HOOK(FlashBat)
{
	CMsgReader msg(pbuf, iSize);
	int flash = msg.readByte();
	if (!msg.readOK())
		return CHAIN_ORIGINAL(FlashBat);
	AutoFunc::TriggerEvent(AutoFunc::EVENTCMD_FLASHBATTERY, std::to_string(flash).c_str());
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

#pragma region AutoFunc
void AutoFunc::Init() {
	s_pCvarAutoJump = abc::CREATE_CVAR("cl_autojump", "0", abc::FCVAR_VALUE, nullptr);
	abc::ADD_COMMAND("+ducktap", []() { s_bAutoDucktap = true; });
	abc::ADD_COMMAND("-ducktap", []() { s_bAutoDucktap = false; });
	//Add concurrent cmd
	abc::ADD_COMMAND("concurrent", []() {
		size_t argc = gEngfuncs.Cmd_Argc();
		if (argc < 3) {
			abc::ConsoleWriteline("concurrent [target command] [concurrent command]\n");
			return;
		}
		SetConcurrent(gEngfuncs.Cmd_Argv(1), gEngfuncs.Cmd_Argv(2));
	});
	abc::ADD_COMMAND("removeconcurrent", []() {
		size_t argc = gEngfuncs.Cmd_Argc();
		if (argc < 2) {
			abc::ConsoleWriteline("remove_concurrent [target command]\n");
			return;
		}
		RemoveConcurrent(gEngfuncs.Cmd_Argv(1));
	});
	//Add events cmd
	abc::ADD_COMMAND("events", []() {
		static auto error = []() {
			abc::ConsoleWriteline("events [operate] [event] [cmd]\n");
			abc::ConsoleWriteline("operate: 0 = delete, 1 = set\n");
			abc::ConsoleWriteline("aviliable events:\n");
			for (auto iter = s_aryEvents.begin(); iter != s_aryEvents.end(); iter++) {
				std::string ss = "    [" + *iter + "]\n";
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
		case 0: {
			if (s_dicEventCmd.find(evt) != s_dicEventCmd.end())
				s_dicEventCmd.erase(evt);
			break;
		}
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
	//Replay concurrent bindings from config (base HUD commands exist by now)
	CAbcEssentialConfig* config = abcess::Config();
	for (auto iter = config->m_dicConcurrentCmd.begin(); iter != config->m_dicConcurrentCmd.end(); iter++) {
		SetConcurrent(iter->first.c_str(), iter->second.c_str());
	}
	s_dicEventCmd = config->m_dicEventsCmd;

	HookMessages();
}

void AutoFunc::Exit() {
	CAbcEssentialConfig* config = abcess::Config();
	config->m_dicConcurrentCmd.clear();
	for (auto iter = s_dicConcurrentCmds.begin(); iter != s_dicConcurrentCmds.end(); iter++) {
		config->m_dicConcurrentCmd.emplace(std::make_pair(iter->first, iter->second->concurrent));
	}
	config->m_dicEventsCmd = s_dicEventCmd;
}

void AutoFunc::AutoJump(usercmd_s* cmd) {
	//Auto jump from openag
	if (s_pCvarAutoJump && s_pCvarAutoJump->value > 0) {
		static bool s_bJumpWasDownLastFrame = false;
		bool shouldReleaseJump = (!s_playerMove.onground && !s_playerMove.inwater && s_playerMove.walking);
		if (s_bJumpWasDownLastFrame && s_playerMove.onground && !s_playerMove.inwater && s_playerMove.walking)
			shouldReleaseJump = true;
		if (shouldReleaseJump)
			cmd->buttons &= ~IN_JUMP;
		s_bJumpWasDownLastFrame = ((cmd->buttons & IN_JUMP) != 0);
	}
}

void AutoFunc::DuckTap(usercmd_s* cmd) {
	//Ducktap from bfhl
	if (s_bAutoDucktap) {
		static bool s_bDuckWasDownLastFrame = false;
		cmd->buttons |= IN_DUCK;
		bool shouldReleaseDuck = (!s_playerMove.onground && !s_playerMove.inwater && s_playerMove.walking);
		if (s_bDuckWasDownLastFrame && s_playerMove.onground && !s_playerMove.inwater && s_playerMove.walking)
			shouldReleaseDuck = true;
		if (shouldReleaseDuck)
			cmd->buttons &= ~IN_DUCK;
		s_bDuckWasDownLastFrame = ((cmd->buttons & IN_DUCK) != 0);
	}
}

void AutoFunc::OnPlayerMove(bool bInWater, bool bOnGround, bool bWalking) {
	s_playerMove.inwater = bInWater;
	s_playerMove.onground = bOnGround;
	s_playerMove.walking = bWalking;
}

void AutoFunc::OnTxferPredictionData(const weapon_data_s* wd) {
	for (int i = 1; i < kMaxWeapons; i++) {
		bool owned = wd[i].m_iId > 0;
		if (owned == s_aryWeaponOwned[i])
			continue;
		s_aryWeaponOwned[i] = owned;
		// scripts key on the weapon name; unknown ids (no WeaponList yet) are skipped
		if (s_aryWeaponNames[i].empty())
			continue;
		TriggerEvent(owned ? EVENTCMD_ADDWEAPON : EVENTCMD_DROPWEAPON, s_aryWeaponNames[i].c_str());
	}
}

void AutoFunc::ResetWeapons() {
	s_aryWeaponNames.fill({});
	s_aryWeaponOwned.fill(false);
}

//death
void AutoFunc::TriggerEvent(const char* eventname,
	const char* param1, const char* param2, const char* param3, const char* param4) {
	if (s_dicEventCmd.find(eventname) != s_dicEventCmd.end()) {
		std::string ssbuf = s_dicEventCmd[eventname];
		if (ssbuf.size() > 0) {
			static auto replace = [](const char* key, const char* value, std::string& ss) {
				if (value) {
					std::string temp1 = ss;
					std::regex pattern(key);
					std::string temp = std::regex_replace(temp1, pattern, value);
					ss = temp;
				}
			};
			replace("\\{param1\\}", param1, ssbuf);
			replace("\\{param2\\}", param2, ssbuf);
			replace("\\{param3\\}", param3, ssbuf);
			replace("\\{param4\\}", param4, ssbuf);
			abc::EngineClientCmd(ssbuf.c_str());
		}
	}
}
#pragma endregion

namespace {
class AutoFuncModule final : public abcess::IFeatureModule {
public:
	AutoFuncModule() { abcess::moduleRegistry().add(this); }
	const char* name() const override { return "autofunc"; }
	void onPostInit() override { AutoFunc::Init(); }
	void onVidInit() override { AutoFunc::ResetWeapons(); }
	void onShutdown() override { AutoFunc::Exit(); }
};
const AutoFuncModule RegisterModule;
}
