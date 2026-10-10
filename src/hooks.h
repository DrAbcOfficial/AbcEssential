#pragma once

// AbcEssential hook spine (plan 04 §1 hooks.cpp).
// Ported from ABCEnchance src/exportfuncs.cpp (FillEngineAddress/FillAddress/
// InstallEngineHook/InstallClientHook) and the cl_refHookfunc_t of
// src/utility/util.h. Only the hooks this plugin consumes survive: the UI
// scans of the old hub (crosshair redraw, VGUI menu routing, EV punch vector,
// NetEase music branch, HUD list, native panel info, sprite frames) are not
// migrated (plan 04 §2 note: UI-related branches are cut).
//
// Chain semantics (R-2): every HUD_XXX export thunk calls its gExportfuncs
// counterpart (the entry that was in the table before us), so plugins loaded
// after AbcEssential keep a working export table.

#include <metahook.h>
#include <com_model.h>
#include <pm_defs.h>
#include <r_studioint.h>

namespace hooks {

constexpr char ABCESS_VERSION[] = "0.2.0";

// Originals + scanned addresses, filled by FillEngineAddress/FillClientAddress
// and detoured by InstallEngineHooks/InstallClientHooks.
//
// Field ownership (R-2, Phase 2 audit): CHAIN-OWNED fields (detoured) may
// only be called from src/hooks.cpp - chaining back to the original is the
// spine's job, modules never touch them (an original the module needs is
// injected as a parameter instead). DIRECT-CALL fields (sig-scanned, no
// detour) carry no chain and may be used by modules freely.
struct SHookedFuncs {
	// chain-owned: engine efx API originals (detoured in place)
	void (*R_BloodSprite)(float* org, int colorindex, int modelIndex, int modelIndex2, float size);
	TEMPENTITY* (*R_TempModel)(float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype);
	// chain-owned: engine engfunc original (detoured)
	void (*pfnPlaybackEvent)(int flags, const struct edict_s* pInvoker, unsigned short eventindex, float delay, float* origin, float* angles,
		float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
	// chain-owned: engine sig-scanned originals (detoured)
	void (*R_NewMap)();
	model_t* (*CL_GetModelByIndex)(int index);
	// chain-owned: client.dll detoured only to capture pClientSoundEngine
	void (__fastcall* CClient_SoundEngine_Initialize)(void* pSoundEngine, int dummy);
	// direct-call: client.dll sig-scanned, no detour
	void (__fastcall* CClient_SoundEngine_PlayFMODSound)(void* pSoundEngine, int dummy, int flags, int entindex, float* origin,
		int channel, const char* name, float fvol, float attenuation, int extraflags, int pitch, int sentenceIndex, float soundLength);
	void (*V_PunchAxis)(int axis, float punch);
	// captured by the CClient_SoundEngine_Initialize detour ("this" of the FMOD wrapper)
	void* pClientSoundEngine;
};
extern SHookedFuncs gHooks;

// engine_studio_api_t as handed to HUD_GetStudioModelInterface; the extra
// precache loads its models through it.
extern engine_studio_api_t gEngineStudio;

// LoadEngine time (no cvar/command registration here - 08 §4.9 red line).
void FillEngineAddress();
void InstallEngineHooks();
void UninstallEngineHooks();
// LoadClient time.
void FillClientAddress();
void InstallClientHooks();
// HUD_Shutdown time.
void UninstallClientHooks();
// LoadClient time: chain our HUD_XXX implementations into the export table.
void AttachExports(cl_exportfuncs_t* pExportFuncs);

}
