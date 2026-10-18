// `models` - list player model folders with persistent favorite marks.
// Ported from ABCEnchance src/exportfuncs.cpp models command block; the old
// command only listed `models/player/<arg>*.*` (bare `models` did nothing)
// and the config's favmodels field had no consumer - favorites are implemented
// here against the plugin config (plan 04 §2/§3, case 3):
//   models              list all (favorites first, marked with *)
//   models <prefix>     list matching
//   models fav <name>   favorite a model
//   models unfav <name> unfavorite a model
// Output markers ("Models", "==============" headers, "* " favorite lines)
// are asserted by external tooling - do not reword.

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <metahook.h>

#include <Interface/IFileSystem.h>

#include <abc/Console.h>
#include <abc/CvarCmd.h>
#include <abc/Plugin.h>

#include "../../services/Config.h"
#include "../Module.h"

namespace models {

namespace {

using abc::g_pFileSystem;

void printModelList(const char *prefix) {
  std::string Wildcard = std::string("models/player/") + prefix + "*.*";
  abc::ConsolePrint("Models\n==============\n");
  FileFindHandle_t FindHandle;
  const char *FileName =
      g_pFileSystem->FindFirst(Wildcard.c_str(), &FindHandle);
  if (!FileName) {
    abc::ConsoleWarn("models: nothing found (%s)\n", Wildcard.c_str());
    abc::ConsolePrint("==============\n");
    return;
  }
  std::vector<std::string> Names;
  do {
    if (g_pFileSystem->FindIsDirectory(FindHandle))
      Names.push_back(FileName);
  } while ((FileName = g_pFileSystem->FindNext(FindHandle)) != nullptr);
  g_pFileSystem->FindClose(FindHandle);

  const std::set<std::string> &Favorites = abcess::config()->FavoriteModels;
  std::sort(Names.begin(), Names.end(),
            [&Favorites](const std::string &a, const std::string &b) {
              bool FavA = Favorites.count(a) != 0;
              bool FavB = Favorites.count(b) != 0;
              if (FavA != FavB)
                return FavA > FavB;
              return a < b;
            });
  for (const auto &Name : Names)
    abc::ConsolePrint("%s %s\n", Favorites.count(Name) ? "*" : " ",
                      Name.c_str());
  abc::ConsolePrint("==============\n");
}

void registerCommand() {
  abc::ADD_COMMAND("models", []() {
    if (gEngfuncs.Cmd_Argc() <= 1) {
      printModelList("");
      return;
    }
    std::string Op = gEngfuncs.Cmd_Argv(1);
    if ((Op == "fav" || Op == "unfav") && gEngfuncs.Cmd_Argc() >= 3) {
      std::string Name = gEngfuncs.Cmd_Argv(2);
      auto &Favorites = abcess::config()->FavoriteModels;
      if (Op == "fav") {
        Favorites.insert(Name);
        abc::ConsolePrint("* %s\n", Name.c_str());
      } else {
        Favorites.erase(Name);
        abc::ConsolePrint("  %s\n", Name.c_str());
      }
      abcess::saveConfig();
      return;
    }
    printModelList(gEngfuncs.Cmd_Argv(1));
  });
}

} // namespace

namespace {

class ModelsModule final : public abcess::IFeatureModule {
public:
  ModelsModule() {
    abcess::moduleRegistry().add(this);
  }
  const char *name() const override {
    return "models";
  }
  void onInit() override {
    registerCommand();
  }
};
const ModelsModule RegisterModule;

} // namespace

} // namespace models
