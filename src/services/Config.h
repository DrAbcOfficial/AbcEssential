#pragma once

// Plugin config schema (plan 04 §3): <mod>/svencoop/abc/abcessential/config.json
// (plugin resources live under the shared abc/ group dir).
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
	// Version zero or unknown versions are preserved without automatic writes.
	// A missing file is initialized to version one by loadConfig.
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
