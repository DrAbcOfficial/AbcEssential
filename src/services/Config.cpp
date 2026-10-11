#include "Config.h"

#include <abc/Config.h>
#include <abc/Console.h>

namespace abcess {
namespace {
constexpr char kConfigPath[] = "abcessential/config.json";
constexpr char kLegacyConfigPath[] = "abcenchance/config.json";

struct LegacyConfig {
	std::set<std::string> FavoriteModels;
	std::map<std::string, std::string> ConcurrentCmds;
	std::map<std::string, std::string> EventsCmds;
	AIGC_JSON_HELPER(FavoriteModels, ConcurrentCmds, EventsCmds);
	AIGC_JSON_HELPER_RENAME("favmodels", "concurrent_cmd", "events_cmd");
};

AbcEssentialConfig ConfigInstance;
bool SaveAllowed = false;
}

AbcEssentialConfig* config() {
	return &ConfigInstance;
}

void loadConfig() {
	SaveAllowed = false;
	ConfigInstance = {};
	const auto Status = abc::JsonLoadWithStatus(ConfigInstance, kConfigPath);
	if (Status == abc::JsonLoadStatus::Success) {
		if (ConfigInstance.SchemaVersion == 1) {
			SaveAllowed = true;
			return;
		}
		ConfigInstance = {};
		abc::ConsoleWarn("[AbcEssential] unsupported config schema; original file preserved, saving disabled\n");
		return;
	}
	if (Status != abc::JsonLoadStatus::Missing) {
		abc::ConsoleWarn("[AbcEssential] config could not be loaded; original file preserved, saving disabled\n");
		return;
	}

	LegacyConfig Legacy;
	const auto LegacyStatus = abc::JsonLoadWithStatus(Legacy, kLegacyConfigPath);
	if (LegacyStatus == abc::JsonLoadStatus::Success) {
		ConfigInstance.FavoriteModels = std::move(Legacy.FavoriteModels);
		ConfigInstance.ConcurrentCmds = std::move(Legacy.ConcurrentCmds);
		ConfigInstance.EventsCmds = std::move(Legacy.EventsCmds);
	} else if (LegacyStatus != abc::JsonLoadStatus::Missing) {
		abc::ConsoleWarn("[AbcEssential] legacy config could not be loaded; import and saving disabled\n");
		return;
	}
	ConfigInstance.SchemaVersion = 1;
	SaveAllowed = true;
	saveConfig();
	if (LegacyStatus == abc::JsonLoadStatus::Success)
		abc::ConsolePrint("[AbcEssential] imported legacy config from %s\n", kLegacyConfigPath);
}

void saveConfig() {
	if (!SaveAllowed)
		return;
	if (!abc::JsonSave(ConfigInstance, kConfigPath))
		abc::ConsoleWarn("[AbcEssential] could not save config; previous file retained\n");
}
}
