#pragma once

// Plugin config schema (plan 04 §3): <mod>/svencoop/abcessential/config.json.
// The framework half (JSON IO through the engine filesystem) is abc::Config;
// only the concrete field names live here. The old ABCEnchance schema
// ("favmodels"/"concurrent_cmd"/"events_cmd" in abcenchance/config.json) is
// imported once when the new file does not exist yet. JSON keys and cvar/
// command names are persisted contracts - never rename them.

#include <AIGCJson.hpp>

#include <map>
#include <set>
#include <string>

namespace abcess {

struct AbcEssentialConfig {
	// 0 = pre-Phase-2 file (or absent); bumped on load so the legacy import
	// runs exactly once even when a Phase 1 acceptance-config already exists.
	int SchemaVersion = 0;
	std::set<std::string> FavoriteModels;
	std::map<std::string, std::string> ConcurrentCmds;
	std::map<std::string, std::string> EventsCmds;

	AIGC_JSON_HELPER(SchemaVersion, FavoriteModels, ConcurrentCmds, EventsCmds);
	AIGC_JSON_HELPER_RENAME("schema_version", "favorite_models", "concurrent_cmds", "event_cmds");
};

// per-DLL singleton (static lib)
AbcEssentialConfig* config();
// OnLoadClient time: load, or one-time import from the legacy ABCEnchance file.
void loadConfig();
// HUD_Shutdown time.
void saveConfig();

}
