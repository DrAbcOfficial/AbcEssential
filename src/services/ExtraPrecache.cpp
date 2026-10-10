#include "ExtraPrecache.h"

#include <cstring>

#include <metahook.h>
#include <com_model.h>
#include <r_studioint.h>

namespace extraPrecache {

namespace {

// out-of-band model index range the engine never produces
constexpr int kIndexBase = 2500000;
constexpr int kMaxModels = 255;
// engine model needload states (com_model.h carries no constants for them;
// NL_PRESENT/NL_NEEDS_LOADED stay unused here)
constexpr int kNeedLoadUnreferenced = 2; // NL_UNREFERENCED
constexpr int kNeedLoadClient = 3;       // NL_CLIENT

model_t* g_ExtraPrecacheModel[kMaxModels] = { 0 };
size_t g_iExtraPrecacheSize = 0;
const engine_studio_api_t* s_pStudio = nullptr;

}

void setEngineStudio(const engine_studio_api_s* studio) {
	s_pStudio = studio;
}

model_t* getByModelIndex(int index) {
	if (index >= kIndexBase && index < kIndexBase + kMaxModels)
		return g_ExtraPrecacheModel[index - kIndexBase];
	return nullptr;
}

int getIndex(const char* path) {
	for (size_t i = 0; i < g_iExtraPrecacheSize; i++) {
		if (!g_ExtraPrecacheModel[i])
			continue;
		if (!strcmp(path, g_ExtraPrecacheModel[i]->name))
			return static_cast<int>(i) + kIndexBase;
	}
	return -1;
}

int precache(const char* path) {
	int index = getIndex(path);
	if (index > -1)
		return index;
	if (!s_pStudio || g_iExtraPrecacheSize >= kMaxModels)
		return -1;
	g_ExtraPrecacheModel[g_iExtraPrecacheSize] = s_pStudio->Mod_ForName(path, true);
	if (!g_ExtraPrecacheModel[g_iExtraPrecacheSize])
		return -1;
	g_ExtraPrecacheModel[g_iExtraPrecacheSize]->needload = kNeedLoadClient;
	index = static_cast<int>(g_iExtraPrecacheSize) + kIndexBase;
	g_iExtraPrecacheSize++;
	return index;
}

void clear() {
	for (size_t i = 0; i < g_iExtraPrecacheSize; i++) {
		if (g_ExtraPrecacheModel[i]) {
			g_ExtraPrecacheModel[i]->needload = kNeedLoadUnreferenced;
			g_ExtraPrecacheModel[i] = nullptr;
		}
	}
	g_iExtraPrecacheSize = 0;
}

}
