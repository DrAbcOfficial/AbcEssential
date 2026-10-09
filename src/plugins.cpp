// AbcEssential - Phase 1 acceptance plugin (plan 03 §4).
// Exercises the abc_core surface end to end and reports through the console:
//   abc_test         console print/warn + cvar roundtrip + registrar listing +
//                    steamid parse + config save/load + background task start +
//                    chained-command subscriber arm
//   abc_test_status  task/continuation state + path helpers
// Load-time output proves plugin base wiring (version string, config load).
// The chained command is hooked on "version" (engine-registered, reachable
// from RCON); running `version` after abc_test must set the subscriber flag
// and still print the engine version (chain-through).
#include <metahook.h>

#include <abc/Plugin.h>
#include <abc/Core.h>
#include <abc/Console.h>
#include <abc/CvarCmd.h>
#include <abc/Config.h>
#include <abc/Paths.h>
#include <abc/Task.h>
#include <abc/Events.h>
#include <abc/SteamId.h>

#include <cstdio>
#include <cstring>

static constexpr char CONFIG_PATH[] = "abcessential/config.json";

struct CAcceptConfig {
	int nLaunchCount = 0;
	std::string szLastGreeting = "hello";

	AIGC_JSON_HELPER(nLaunchCount, szLastGreeting);
	AIGC_JSON_HELPER_RENAME("launch_count", "last_greeting");
};

static CAcceptConfig s_Cfg;
static cvar_t* s_pCvarTest = nullptr;
static abc::CChainedCommand* s_pChainedVersion = nullptr;
static bool s_bTaskDone = false;
static int s_nTaskResult = -1;
static bool s_bChainSubscriberRan = false;

static void HUD_Frame(double time);
static void HUD_Init(void);

static void CmdStatus() {
	abc::ConsolePrint("[abc_test_status] task %s (result=%d), chain subscriber %s\n",
		s_bTaskDone ? "DONE" : "pending", s_nTaskResult,
		s_bChainSubscriberRan ? "RAN" : "not yet");
	abc::ConsolePrint("[abc_test_status] gamedir=%s\n", abc::GameDir());
	abc::ConsolePrint("[abc_test_status] config=%s\n", abc::ModPath(CONFIG_PATH).c_str());
	abc::ConsolePrint("[abc_test_status] cvar abc_test_value=%s (pointer=%p)\n",
		abc::CVAR_GET_STRING("abc_test_value"), (void*)abc::CVAR_GET_POINTER("abc_test_value"));
}

static void CmdTest() {
	abc::ConsolePrint("[abc_test] 1 console print ok, core=%s\n", abc::CoreVersion());
	abc::ConsoleWarn("2 console warn ok\n");

	abc::CVAR_SET_FLOAT("abc_test_value", 3.5f);
	float v = abc::CVAR_GET_FLOAT("abc_test_value");
	abc::ConsolePrint("[abc_test] 3 cvar roundtrip %.2f -> %s\n", v, v == 3.5f ? "ok" : "FAIL");

	abc::ConsolePrint("[abc_test] 4 registrar:\n");
	abc::CCvarRegistrar::Get()->ListAll();

	CSteamID id = abc::ParseSteamID("[U:1:1234]");
	const char* rendered = abc::RenderSteamID(id);
	abc::ConsolePrint("[abc_test] 5 steamid [U:1:1234] -> %s -> %s\n", rendered,
		strcmp(rendered, "[U:1:1234]") == 0 ? "ok" : "FAIL");

	s_Cfg.szLastGreeting = "abc_test-run";
	bool saved = abc::JsonSave(s_Cfg, CONFIG_PATH);
	CAcceptConfig check;
	bool loaded = abc::JsonLoad(check, CONFIG_PATH);
	bool ok = saved && loaded && check.szLastGreeting == "abc_test-run" && check.nLaunchCount == s_Cfg.nLaunchCount;
	abc::ConsolePrint("[abc_test] 6 config save=%d load=%d (launch_count=%d greeting=%s) -> %s\n",
		saved, loaded, check.nLaunchCount, check.szLastGreeting.c_str(), ok ? "ok" : "FAIL");

	s_bTaskDone = false;
	s_nTaskResult = -1;
	auto task = abc::GetTaskManager()->Add<int>([]() -> int {
		// background thread work; result must surface via HUD_Frame pump
		int sum = 0;
		for (int i = 1; i <= 6; i++) {
			Sleep(50);
			sum += i;
		}
		return sum;
	});
	task->ContinueWith([](int& result) {
		s_nTaskResult = result;
		s_bTaskDone = true;
	});
	abc::ConsolePrint("[abc_test] 7 task started (expect 21; poll abc_test_status)\n");

	s_bChainSubscriberRan = false;
	s_pChainedVersion->Event().append([]() {
		// Returning true lets the original `version` run: chain-through proof.
		s_bChainSubscriberRan = true;
		return true;
	});
	abc::ConsolePrint("[abc_test] 8 chained-command subscriber armed; run `version` then abc_test_status\n");
}

class CAbcEssential : public abc::CAbcPluginBase {
public:
	CAbcEssential() : abc::CAbcPluginBase("AbcEssential", "0.1.0") {}

protected:
	void OnLoadEngine() override {
		// Registration is deferred to HUD_Init: the engine cvar table is not
		// ready during LoadEngine (crash reproduced and traced 2026-10-09).
	}

	void OnLoadClient(cl_exportfuncs_t* pExportFuncs) override {
		pExportFuncs->HUD_Init = HUD_Init;
		pExportFuncs->HUD_Frame = HUD_Frame;

		if (!abc::JsonLoad(s_Cfg, CONFIG_PATH))
			abc::ConsoleWarn("config missing, starting fresh (%s)\n", CONFIG_PATH);
		s_Cfg.nLaunchCount += 1;
		abc::JsonSave(s_Cfg, CONFIG_PATH);

		abc::ConsolePrint("[AbcEssential] loaded %s (launch #%d)\n", Version(), s_Cfg.nLaunchCount);
	}

	void OnExitGame(int iResult) override {
		abc::GetTaskManager()->Shutdown();
	}
};

static void HUD_Init(void) {
	static bool s_bRegistered = false;
	if (!s_bRegistered) {
		s_bRegistered = true;
		// cvar/command registration happens at HUD_Init time (client ready),
		// the proven point from ABCEnchance.
		s_pCvarTest = abc::CCvarRegistrar::Get()->Create("abc_test_value", "1.0", abc::FCVAR_VALUE);
		abc::ADD_COMMAND("abc_test", CmdTest);
		abc::ADD_COMMAND("abc_test_status", CmdStatus);
		s_pChainedVersion = new abc::CChainedCommand("version");
		abc::ConsolePrint("[AbcEssential] cvar + commands ready\n");
	}
	gExportfuncs.HUD_Init();
}

static void HUD_Frame(double time) {
	// Pump main-thread continuations (abc::Task / DispatchToMainThread).
	abc::GetTaskManager()->CheckAll();
	gExportfuncs.HUD_Frame(time);
}

EXPOSE_SINGLE_INTERFACE(CAbcEssential, IPluginsV4, METAHOOK_PLUGIN_API_VERSION_V4);
