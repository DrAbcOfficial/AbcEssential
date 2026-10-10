#include "Config.h"

#include <abc/Config.h>
#include <abc/Console.h>

namespace abcess {

namespace {

constexpr char kConfigPath[] = "abcessential/config.json";
constexpr char kLegacyConfigPath[] = "abcenchance/config.json";

// old ABCEnchance field names (src/core/module/config.h)
struct LegacyConfig {
	std::set<std::string> FavoriteModels;
	std::map<std::string, std::string> ConcurrentCmds;
	std::map<std::string, std::string> EventsCmds;

	AIGC_JSON_HELPER(FavoriteModels, ConcurrentCmds, EventsCmds);
	AIGC_JSON_HELPER_RENAME("favmodels", "concurrent_cmd", "events_cmd");
};

AbcEssentialConfig ConfigInstance;

}

AbcEssentialConfig* config() {
	return &ConfigInstance;
}

void loadConfig() {
	// A pre-Phase-2 file (Phase 1 acceptance config or foreign schema) parses
	// fine but carries schema_version 0: rerun the legacy import for it too.
	if (abc::JsonLoad(ConfigInstance, kConfigPath) && ConfigInstance.SchemaVersion >= 1)
		return;

	// One-time import: read the old abcenchance/config.json, write the new
	// file. The legacy file is kept - the old plugin may still be installed.
	LegacyConfig legacy;
	if (abc::JsonLoad(legacy, kLegacyConfigPath)) {
		ConfigInstance.FavoriteModels = std::move(legacy.FavoriteModels);
		ConfigInstance.ConcurrentCmds = std::move(legacy.ConcurrentCmds);
		ConfigInstance.EventsCmds = std::move(legacy.EventsCmds);
		ConfigInstance.SchemaVersion = 1;
		saveConfig();
		abc::ConsolePrint("[AbcEssential] imported legacy config from %s\n", kLegacyConfigPath);
		return;
	}
	if (ConfigInstance.SchemaVersion < 1) {
		// nothing to import; still stamp the schema so the import never re-runs
		ConfigInstance.SchemaVersion = 1;
		saveConfig();
	}
	abc::ConsoleWarn("[AbcEssential] no legacy config to import, starting fresh (%s)\n", kConfigPath);
}

void saveConfig() {
	abc::JsonSave(ConfigInstance, kConfigPath);
}

}
