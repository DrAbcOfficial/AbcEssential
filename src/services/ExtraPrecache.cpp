#include "ExtraPrecache.h"

#include <cstring>

#include <metahook.h>
#include <com_model.h>
#include <r_studioint.h>

namespace extraprecache {

namespace {

// out-of-band model index range the engine never produces
constexpr int kIndexBase = 2500000;
constexpr int kMaxModels = 255;
// engine model needload states (com_model.h carries no constants for them;
// NL_PRESENT/NL_NEEDS_LOADED stay unused here)
constexpr int kNeedLoadUnreferenced = 2; // NL_UNREFERENCED
constexpr int kNeedLoadClient = 3;       // NL_CLIENT

model_t* PrecachedModels[kMaxModels] = {};
int ModelCount = 0;
const engine_studio_api_t* Studio = nullptr;

}

void setEngineStudio(const engine_studio_api_s* studio) {
	Studio = studio;
}

model_t* getByModelIndex(int index) {
	if (index >= kIndexBase && index < kIndexBase + kMaxModels)
		return PrecachedModels[index - kIndexBase];
	return nullptr;
}

int getIndex(const char* path) {
	for (int i = 0; i < ModelCount; i++) {
		if (!PrecachedModels[i])
			continue;
		if (!strcmp(path, PrecachedModels[i]->name))
			return i + kIndexBase;
	}
	return -1;
}

int precache(const char* path) {
	int index = getIndex(path);
	if (index > -1)
		return index;
	if (!Studio || ModelCount >= kMaxModels)
		return -1;
	PrecachedModels[ModelCount] = Studio->Mod_ForName(path, true);
	if (!PrecachedModels[ModelCount])
		return -1;
	PrecachedModels[ModelCount]->needload = kNeedLoadClient;
	index = ModelCount + kIndexBase;
	ModelCount++;
	return index;
}

void clear() {
	for (int i = 0; i < ModelCount; i++) {
		if (PrecachedModels[i]) {
			PrecachedModels[i]->needload = kNeedLoadUnreferenced;
			PrecachedModels[i] = nullptr;
		}
	}
	ModelCount = 0;
}

}
