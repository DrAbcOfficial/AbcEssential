// AbcEssential - non-UI utility plugin (plan 04).
// find / models / autofunc (autojump, ducktap, concurrent, event scripts) /
// viewmodel lag + camera / efx enhancements. Zero rendering dependencies: no
// IMetaRenderer and no Renderer.dll requirement (04 §5 case 10).
//
// Phase 1's abc_test acceptance harness was retired with this rewrite; the
// Phase 2 acceptance is the ABCEnchance feature surface itself (04 §5).
#include <metahook.h>

#include <abc/Console.h>
#include <abc/Plugin.h>
#include <abc/Task.h>

#include "hooks.h"
#include "services/Config.h"
#include "services/Version.h"

class CAbcEssential : public abc::CAbcPluginBase {
public:
	CAbcEssential() : abc::CAbcPluginBase("AbcEssential", abcess::kVersion) {}

protected:
	void OnLoadEngine() override {
		// engine sig scans + efx/engfunc inline hooks; no cvar/command
		// registration here (LoadEngine-time registration crashes, 08 §4.9)
		hooks::FillEngineAddress();
		hooks::InstallEngineHooks();
	}

	void OnLoadClient(cl_exportfuncs_t* pExportFuncs) override {
		hooks::FillClientAddress();
		hooks::InstallClientHooks();
		hooks::AttachExports(pExportFuncs);
		abcess::LoadConfig();
		abc::ConsolePrint("[AbcEssential] loaded %s\n", Version());
	}

	void OnExitGame(int iResult) override {
		hooks::UninstallEngineHooks();
		abc::GetTaskManager()->Shutdown();
	}
};

EXPOSE_SINGLE_INTERFACE(CAbcEssential, IPluginsV4, METAHOOK_PLUGIN_API_VERSION_V4);
