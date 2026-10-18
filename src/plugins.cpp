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

class AbcEssentialPlugin : public abc::CAbcPluginBase {
public:
  AbcEssentialPlugin()
      : abc::CAbcPluginBase("AbcEssential", abcess::kVersion) {}

protected:
  void OnLoadEngine() override {
    // engine sig scans + efx/engfunc inline hooks; no cvar/command
    // registration here (LoadEngine-time registration crashes, 08 §4.9)
    hooks::fillEngineAddress();
    hooks::installEngineHooks();
  }

  void OnLoadClient(cl_exportfuncs_t *pExportFuncs) override {
    hooks::fillClientAddress();
    hooks::installClientHooks();
    hooks::attachExports(pExportFuncs);
    abcess::loadConfig();
    abc::ConsolePrint("[AbcEssential] loaded %s\n", Version());
  }

  void OnExitGame(int iResult) override {
    (void)iResult;
    // HUD_Shutdown normally removes the client detour first; ExitGame can
    // bypass it, and uninstallClientHooks is idempotent.
    hooks::uninstallClientHooks();
    hooks::uninstallEngineHooks();
    abc::GetTaskManager()->Shutdown();
  }
};

EXPOSE_SINGLE_INTERFACE(AbcEssentialPlugin, IPluginsV4,
                        METAHOOK_PLUGIN_API_VERSION_V4);
