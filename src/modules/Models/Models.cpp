// `models` - list player model folders with persistent favorite marks.
// Ported from ABCEnchance src/exportfuncs.cpp models command block; the old
// command only listed `models/player/<arg>*.*` (bare `models` did nothing)
// and the config's favmodels field had no consumer - favorites are implemented
// here against the plugin config (plan 04 §2/§3, case 3):
//   models              list all (favorites first, marked with *)
//   models <prefix>     list matching
//   models fav <name>   favorite a model
//   models unfav <name> unfavorite a model
#include "Models.h"

#include <algorithm>
#include <string>
#include <vector>

#include <metahook.h>

#include <Interface/IFileSystem.h>

#include <abc/CvarCmd.h>
#include <abc/Plugin.h>

#include "../../services/Config.h"
#include "../Module.h"

namespace {

void PrintModelList(const char* prefix) {
	std::string wildcard = std::string("models/player/") + prefix + "*.*";
	gEngfuncs.Con_Printf("Models\n==============\n");
	FileFindHandle_t walk;
	const char* pszFileName = abc::g_pFileSystem->FindFirst(wildcard.c_str(), &walk);
	if (!pszFileName) {
		gEngfuncs.Con_Printf("!!!Nothing!!!\n==============\n");
		return;
	}
	std::vector<std::string> names;
	do {
		if (abc::g_pFileSystem->FindIsDirectory(walk))
			names.push_back(pszFileName);
	} while ((pszFileName = abc::g_pFileSystem->FindNext(walk)) != nullptr);
	abc::g_pFileSystem->FindClose(walk);

	const auto& favs = abcess::Config()->m_aryFavModels;
	std::sort(names.begin(), names.end(), [&favs](const std::string& a, const std::string& b) {
		bool fa = favs.count(a) != 0, fb = favs.count(b) != 0;
		if (fa != fb)
			return fa > fb;
		return a < b;
	});
	for (const auto& name : names)
		gEngfuncs.Con_Printf("%s %s\n", favs.count(name) ? "*" : " ", name.c_str());
	gEngfuncs.Con_Printf("==============\n");
}

}

void Models::Init() {
	abc::ADD_COMMAND("models", []() {
		if (gEngfuncs.Cmd_Argc() <= 1) {
			PrintModelList("");
			return;
		}
		std::string op = gEngfuncs.Cmd_Argv(1);
		if ((op == "fav" || op == "unfav") && gEngfuncs.Cmd_Argc() >= 3) {
			std::string name = gEngfuncs.Cmd_Argv(2);
			auto& favs = abcess::Config()->m_aryFavModels;
			if (op == "fav") {
				favs.insert(name);
				gEngfuncs.Con_Printf("* %s\n", name.c_str());
			}
			else {
				favs.erase(name);
				gEngfuncs.Con_Printf("  %s\n", name.c_str());
			}
			abcess::SaveConfig();
			return;
		}
		PrintModelList(gEngfuncs.Cmd_Argv(1));
	});
}

namespace {
class ModelsModule final : public abcess::IFeatureModule {
public:
	ModelsModule() { abcess::moduleRegistry().add(this); }
	const char* name() const override { return "models"; }
	void onInit() override { Models::Init(); }
};
const ModelsModule RegisterModule;
}
