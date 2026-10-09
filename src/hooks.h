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
#include <r_studioint.h>

namespace hooks {

constexpr char ABCESS_VERSION[] = "0.2.0";

// Originals + scanned addresses, filled by FillEngineAddress/FillClientAddress
// and detoured by InstallEngineHooks/InstallClientHooks.
struct SHookedFuncs {
	// engine efx API originals (detoured in place)
	void (*R_BloodSprite)(float* org, int colorindex, int modelIndex, int modelIndex2, float size);
	TEMPENTITY* (*R_TempModel)(float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype);
	// engine engfuncs original (detoured)
	void (*pfnPlaybackEvent)(int flags, const struct edict_s* pInvoker, unsigned short eventindex, float delay, float* origin, float* angles,
		float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
	// engine sig-scanned originals (detoured)
	void (*R_NewMap)();
	model_t* (*CL_GetModelByIndex)(int index);
	// client.dll sig-scanned, direct call only (no detour needed)
	void (*V_PunchAxis)(int axis, float punch);
	void (__fastcall* CClient_SoundEngine_PlayFMODSound)(void* pSoundEngine, int dummy, int flags, int entindex, float* origin,
		int channel, const char* name, float fvol, float attenuation, int extraflags, int pitch, int sentenceIndex, float soundLength);
	// client.dll sig-scanned, detoured only to capture pClientSoundEngine
	void (__fastcall* CClient_SoundEngine_Initialize)(void* pSoundEngine, int dummy);
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
