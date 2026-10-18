// `find` - fuzzy-search cvars and console commands by substring.
// Ported from ABCEnchance src/exportfuncs.cpp HUD_Init find block (~308-357).
// Protected cvars print "**" instead of their value.
// Output markers ("  name  [value]", "======" separators) are asserted by
// external tooling - do not reword.

#include <string>

#include <metahook.h>

#include <abc/Console.h>
#include <abc/CvarCmd.h>

#include "../Module.h"

namespace find {

namespace {

void registerCommand() {
  abc::ADD_COMMAND("find", []() {
    if (gEngfuncs.Cmd_Argc() <= 1)
      return;
    std::string query = gEngfuncs.Cmd_Argv(1);
    abc::ConsolePrint("==============\n");
    for (cvar_t *cvar = gEngfuncs.GetFirstCvarPtr(); cvar; cvar = cvar->next) {
      std::string name = cvar->name;
      if (name.find(query) != std::string::npos)
        abc::ConsolePrint("  %s  [%s]\n", cvar->name,
                          (cvar->flags & FCVAR_PROTECTED) != 0 ? "**"
                                                               : cvar->string);
    }
    abc::ConsolePrint("==============\n");
    for (unsigned int handle = gEngfuncs.GetFirstCmdFunctionHandle(); handle;
         handle = gEngfuncs.GetNextCmdFunctionHandle(handle)) {
      std::string name = gEngfuncs.GetCmdFunctionName(handle);
      if (name.find(query) != std::string::npos)
        abc::ConsolePrint("  %s\n", name.c_str());
    }
    abc::ConsolePrint("==============\n");
  });
}

} // namespace

namespace {

class FindModule final : public abcess::IFeatureModule {
public:
  FindModule() {
    abcess::moduleRegistry().add(this);
  }
  const char *name() const override {
    return "find";
  }
  void onInit() override {
    registerCommand();
  }
};
const FindModule RegisterModule;

} // namespace

} // namespace find
