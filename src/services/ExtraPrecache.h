#pragma once

// Out-of-band model indexes (>= kIndexBase) for models the plugin loads
// itself (gauss beams, egon ball...). Resolved back by the CL_GetModelByIndex
// detour. Ported from ABCEnchance src/core/module/extraprecache.*.
// Plugin-level service (also consumed by the spine), not an efx detail -
// hence src/services/. The studio API is pushed in by the spine at
// HUD_GetStudioModelInterface time; this service has no include of the spine.

struct model_s;
struct engine_studio_api_s;

namespace extraprecache {

// Capture the studio API used to load out-of-band models.
void setEngineStudio(const engine_studio_api_s *studio);

model_s *getByModelIndex(int index);
int getIndex(const char *path);
int precache(const char *path);
// R_NewMap: release every out-of-band model (per-map lifetimes).
void clear();

} // namespace extraprecache
