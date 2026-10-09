#pragma once

#include <metahook.h>
#include <com_model.h>

#define NL_PRESENT 0
#define NL_NEEDS_LOADED 1
#define NL_UNREFERENCED 2
#define NL_CLIENT 3

#define MAX_EXTRA_PRECACHENUM 255
#define EXTRAPRECACHE_INDEX_BASE 2500000

// Out-of-band model indexes (>= 2500000) for models the plugin loads itself
// (gauss beams, egon ball...). Resolved back by the CL_GetModelByIndex detour.
// Ported from ABCEnchance src/core/module/extraprecache.*.
model_t* ExtraPrecache_GetByModelIndex(int index);
int ExtraPrecache_GetIndex(const char* path);
int ExtraPrecache_Precache(const char* path);
void ExtraPrecache_Clear();
