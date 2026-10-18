#pragma once

struct cl_entity_s;
struct edict_s;
struct tempent_s;
using TEMPENTITY = struct tempent_s;

namespace fx {

// The R_TempModel entry as scanned by the spine, injected by src/hooks.cpp:
// the smoke shell must spawn through the engine API, but modules never call
// the hook chain themselves (R-2 - chaining is the spine's job).
using TempModelFn = TEMPENTITY *(*)(float *pos, float *dir, float *angles,
                                    float life, int modelIndex, int soundtype);

// Registers abc_bloodefx*/abc_shellefx*/abc_gaussefx/abc_egonefx cvars
// (HUD_Init time).
void init();
// R_NewMap: clear + re-precache the extra models (per-map lifetimes).
void newMap();
// HUD_Frame pump: egon particle cadence.
void frame();

// Hook bodies wired by src/hooks.cpp; the spine chains every original.
// Pure enhancement pass; the spine calls the original afterwards.
void bloodSprite(float *org, int colorindex, int modelIndex, int modelIndex2,
                 float size);
// Enhancement pass around the injected original.
TEMPENTITY *tempModel(float *pos, float *dir, float *angles, float life,
                      int modelIndex, int soundtype, TempModelFn original);
// Returns true when the event was handled (gauss primary fire); the spine
// chains the original only on false.
bool playbackEvent(int flags, const struct edict_s *pInvoker,
                   unsigned short eventindex, float delay, float *origin,
                   float *angles, float fparam1, float fparam2, int iparam1,
                   int iparam2, int bparam1, int bparam2);
// HUD_AddEntity pre-filter: egon beam particle recognition. Returns true when
// the entity must be swallowed (the spine returns 0 to the engine).
bool addEntity(int type, struct cl_entity_s *ent);

} // namespace fx
