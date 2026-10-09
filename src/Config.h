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
	std::set<std::string> m_aryFavModels;
	std::map<std::string, std::string> m_dicConcurrentCmd;
	std::map<std::string, std::string> m_dicEventsCmd;

	AIGC_JSON_HELPER(m_aryFavModels, m_dicConcurrentCmd, m_dicEventsCmd);
	AIGC_JSON_HELPER_RENAME("favorite_models", "concurrent_cmds", "event_cmds");
};

namespace abcess {

// per-DLL singleton (static lib)
CAbcEssentialConfig* Config();
// OnLoadClient time: load, or one-time import from the legacy ABCEnchance file.
void LoadConfig();
// HUD_Shutdown time.
void SaveConfig();

}
