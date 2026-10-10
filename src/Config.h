#pragma once

// Plugin config schema (plan 04 §3): <mod>/svencoop/abcessential/config.json.
// The framework half (JSON IO through the engine filesystem) is abc::Config;
// only the concrete field names live here. The old ABCEnchance schema
// ("favmodels"/"concurrent_cmd"/"events_cmd" in abcenchance/config.json) is
// imported once when the new file does not exist yet.

#include <AIGCJson.hpp>

#include <map>
#include <set>
#include <string>

struct CAbcEssentialConfig {
	// 0 = pre-Phase-2 file (or absent); bumped on load so the legacy import
	// runs exactly once even when a Phase 1 acceptance-config already exists.
	int m_iSchemaVersion = 0;
	std::set<std::string> m_aryFavModels;
	std::map<std::string, std::string> m_dicConcurrentCmd;
	std::map<std::string, std::string> m_dicEventsCmd;

	AIGC_JSON_HELPER(m_iSchemaVersion, m_aryFavModels, m_dicConcurrentCmd, m_dicEventsCmd);
	AIGC_JSON_HELPER_RENAME("schema_version", "favorite_models", "concurrent_cmds", "event_cmds");
};

namespace abcess {

// per-DLL singleton (static lib)
CAbcEssentialConfig* Config();
// OnLoadClient time: load, or one-time import from the legacy ABCEnchance file.
void LoadConfig();
// HUD_Shutdown time.
void SaveConfig();

}
