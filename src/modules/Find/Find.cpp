// `find` - fuzzy-search cvars and console commands by substring.
// Ported from ABCEnchance src/exportfuncs.cpp HUD_Init find block (~308-357).
// Protected cvars print "**" instead of their value.
#include "Find.h"

#include <string>

#include <metahook.h>

#include <abc/Console.h>
#include <abc/CvarCmd.h>

#include "../Module.h"

void Find::Init() {
	abc::ADD_COMMAND("find", []() {
		if (gEngfuncs.Cmd_Argc() <= 1)
			return;
		std::string sz = gEngfuncs.Cmd_Argv(1);
		abc::ConsolePrint("==============\n");
		cvar_t* cvar = gEngfuncs.GetFirstCvarPtr();
		while (cvar) {
			std::string cvarname = cvar->name;
			if (cvarname.find(sz) != std::string::npos)
				abc::ConsolePrint("  %s  [%s]\n", cvar->name,
					(cvar->flags & FCVAR_PROTECTED) != 0 ? "**" : cvar->string);
			cvar = cvar->next;
		}
		abc::ConsolePrint("==============\n");
		unsigned int cmd = gEngfuncs.GetFirstCmdFunctionHandle();
		while (cmd) {
			std::string cmdname = gEngfuncs.GetCmdFunctionName(cmd);
			if (cmdname.find(sz) != std::string::npos)
				abc::ConsolePrint("  %s\n", cmdname.c_str());
			cmd = gEngfuncs.GetNextCmdFunctionHandle(cmd);
		}
		abc::ConsolePrint("==============\n");
	});
}

namespace {
class FindModule final : public abcess::IFeatureModule {
public:
	FindModule() { abcess::moduleRegistry().add(this); }
	const char* name() const override { return "find"; }
	void onInit() override { Find::Init(); }
};
const FindModule RegisterModule;
}
