#include "Config.h"

#include <abc/Config.h>
#include <abc/Console.h>

namespace {

constexpr char CONFIG_PATH[] = "abcessential/config.json";
constexpr char LEGACY_CONFIG_PATH[] = "abcenchance/config.json";

CAbcEssentialConfig s_cfg;

// old ABCEnchance field names (src/core/module/config.h)
struct CLegacyConfig {
	std::set<std::string> m_aryFavModels;
	std::map<std::string, std::string> m_dicConcurrentCmd;
	std::map<std::string, std::string> m_dicEventsCmd;

	AIGC_JSON_HELPER(m_aryFavModels, m_dicConcurrentCmd, m_dicEventsCmd);
	AIGC_JSON_HELPER_RENAME("favmodels", "concurrent_cmd", "events_cmd");
};

}

namespace abcess {

CAbcEssentialConfig* Config() {
	return &s_cfg;
}

void LoadConfig() {
	// A pre-Phase-2 file (Phase 1 acceptance config or foreign schema) parses
	// fine but carries schema_version 0: rerun the legacy import for it too.
	if (abc::JsonLoad(s_cfg, CONFIG_PATH) && s_cfg.m_iSchemaVersion >= 1)
		return;

	// One-time import: read the old abcenchance/config.json, write the new
	// file. The legacy file is kept - the old plugin may still be installed.
	CLegacyConfig legacy;
	if (abc::JsonLoad(legacy, LEGACY_CONFIG_PATH)) {
		s_cfg.m_aryFavModels = std::move(legacy.m_aryFavModels);
		s_cfg.m_dicConcurrentCmd = std::move(legacy.m_dicConcurrentCmd);
		s_cfg.m_dicEventsCmd = std::move(legacy.m_dicEventsCmd);
		s_cfg.m_iSchemaVersion = 1;
		SaveConfig();
		abc::ConsolePrint("[AbcEssential] imported legacy config from %s\n", LEGACY_CONFIG_PATH);
		return;
	}
	if (s_cfg.m_iSchemaVersion < 1) {
		// nothing to import; still stamp the schema so the import never re-runs
		s_cfg.m_iSchemaVersion = 1;
		SaveConfig();
	}
	abc::ConsoleWarn("[AbcEssential] no legacy config to import, starting fresh (%s)\n", CONFIG_PATH);
}

void SaveConfig() {
	abc::JsonSave(s_cfg, CONFIG_PATH);
}

}
