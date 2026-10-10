// AutoFunc: movement automation (autojump / ducktap) and the concurrent
// command chain. Ported from ABCEnchance src/core/module/autofunc.* (GBK ->
// UTF-8). Event scripts and their UserMsg signal feeds live in Signals.* -
// split out in the Phase 2 audit; the old build derived them from core/events.

#include "AutoFunc.h"

#include <map>
#include <string>

#include <metahook.h>

#include <usercmd.h>
#include <in_buttons.h>

#include <abc/CvarCmd.h>
#include <abc/Console.h>

#include "../../Config.h"
#include "../Module.h"
#include "Signals.h"

namespace AutoFunc {

namespace {

cvar_t* s_pCvarAutoJump = nullptr;
bool s_bAutoDucktap = false;

// concurrent chain: when <cmd> runs, its <concurrent> string is issued first,
// then the original entry point. HookCmd has no uninstall - restoring the
// saved original re-wires the engine command table back.
struct concurrentcmd_t {
	std::string cmd;
	std::string concurrent;
	xcommand_t origincall = nullptr;
};
std::map<std::string, concurrentcmd_t> s_dicConcurrentCmds;

using playermove_t = struct {
	bool inwater;
	bool onground;
	bool walking;
};
playermove_t s_playerMove = {};

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
	auto found = s_dicConcurrentCmds.find(cmd);
	if (found == s_dicConcurrentCmds.end()) {
		concurrentcmd_t obj;
		obj.cmd = cmd;
		obj.concurrent = std::move(ss);
		obj.origincall = g_pMetaHookAPI->HookCmd(cmd, []() {
			const char* nowcmd = gEngfuncs.Cmd_Argv(0);
			auto iter = s_dicConcurrentCmds.find(nowcmd);
			if (iter != s_dicConcurrentCmds.end()) {
				gEngfuncs.pfnClientCmd(iter->second.concurrent.c_str());
				iter->second.origincall();
			}
		});
		s_dicConcurrentCmds.emplace(cmd, std::move(obj));
	}
	else {
		found->second.concurrent = concurrent;
	}
}

void RemoveConcurrent(const char* cmd) {
	auto found = s_dicConcurrentCmds.find(cmd);
	if (found != s_dicConcurrentCmds.end()) {
		g_pMetaHookAPI->HookCmd(found->second.cmd.c_str(), found->second.origincall);
		s_dicConcurrentCmds.erase(found);
	}
	else
		abc::ConsoleWriteline("There is no concurrent in this command!\n");
}

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
	//Replay concurrent bindings from config (base HUD commands exist by now)
	CAbcEssentialConfig* config = abcess::Config();
	for (auto iter = config->m_dicConcurrentCmd.begin(); iter != config->m_dicConcurrentCmd.end(); iter++) {
		SetConcurrent(iter->first.c_str(), iter->second.c_str());
	}
}

void AutoFunc::Exit() {
	CAbcEssentialConfig* config = abcess::Config();
	config->m_dicConcurrentCmd.clear();
	for (auto iter = s_dicConcurrentCmds.begin(); iter != s_dicConcurrentCmds.end(); iter++) {
		config->m_dicConcurrentCmd.emplace(std::make_pair(iter->first, iter->second.concurrent));
	}
}

// Shared air-state edge for autojump/ducktap: the button is released every
// airborne frame (a held button re-fires on landing); on the ground it is
// released when it was down the frame before (the tap).
static bool ShouldReleaseButton(bool bWasDown) {
	if (s_playerMove.inwater || !s_playerMove.walking)
		return false;
	return !s_playerMove.onground || bWasDown;
}

void AutoFunc::AutoJump(usercmd_s* cmd) {
	//Auto jump from openag
	if (s_pCvarAutoJump && s_pCvarAutoJump->value > 0) {
		static bool s_bJumpWasDownLastFrame = false;
		if (ShouldReleaseButton(s_bJumpWasDownLastFrame))
			cmd->buttons &= ~IN_JUMP;
		s_bJumpWasDownLastFrame = ((cmd->buttons & IN_JUMP) != 0);
	}
}

void AutoFunc::DuckTap(usercmd_s* cmd) {
	//Ducktap from bfhl
	if (s_bAutoDucktap) {
		static bool s_bDuckWasDownLastFrame = false;
		cmd->buttons |= IN_DUCK;
		if (ShouldReleaseButton(s_bDuckWasDownLastFrame))
			cmd->buttons &= ~IN_DUCK;
		s_bDuckWasDownLastFrame = ((cmd->buttons & IN_DUCK) != 0);
	}
}

void AutoFunc::OnPlayerMove(bool bInWater, bool bOnGround, bool bWalking) {
	s_playerMove.inwater = bInWater;
	s_playerMove.onground = bOnGround;
	s_playerMove.walking = bWalking;
}
#pragma endregion

namespace {
class AutoFuncModule final : public abcess::IFeatureModule {
public:
	AutoFuncModule() { abcess::moduleRegistry().add(this); }
	const char* name() const override { return "autofunc"; }
	void onPostInit() override {
		AutoFunc::Init();
		Signals::Init();
	}
	void onVidInit() override { Signals::ResetWeapons(); }
	void onShutdown() override {
		AutoFunc::Exit();
		Signals::Exit();
	}
};
const AutoFuncModule RegisterModule;
}

}
