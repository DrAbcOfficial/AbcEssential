#pragma once

struct cl_entity_s;
struct edict_s;
struct tempent_s;
using TEMPENTITY = struct tempent_s;

namespace Efx {

// Registers abc_bloodefx*/abc_shellefx*/abc_gaussefx/abc_egonefx cvars
// (HUD_Init time).
void Init();
// R_NewMap: clear + re-precache the extra models (per-map lifetimes).
void Reset();
// HUD_Frame pump: egon particle cadence.
void Frame();

// Hook bodies chained by src/hooks.cpp (originals via hooks::gHooks).
void R_BloodSprite(float* org, int colorindex, int modelIndex, int modelIndex2, float size);
TEMPENTITY* R_TempModel(float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype);
void pfnPlaybackEvent(int flags, const struct edict_s* pInvoker, unsigned short eventindex, float delay,
	float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
// HUD_AddEntity pre-filter: egon beam particle recognition. Returns true when
// the entity must be swallowed (return 0 to the engine).
bool AddEntity(int type, struct cl_entity_s* ent);

}
