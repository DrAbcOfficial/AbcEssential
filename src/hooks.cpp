// AbcEssential hook installation and chained export thunks (plan 04 §1).
// Ported from ABCEnchance src/exportfuncs.cpp; module-specific hook bodies
// are added by the feature modules (modules/) into the seams below.
#include "hooks.h"

#include <vector>

#include <abc/Console.h>
#include <abc/CvarCmd.h>
#include <abc/Plugin.h>
#include <abc/Task.h>

#include "Config.h"
#include "modules/Module.h"
#include "modules/AutoFunc/AutoFunc.h"
#include "modules/Camera/Camera.h"
#include "modules/Efx/Efx.h"
#include "modules/Efx/ExtraPrecache.h"
#include "modules/ViewModelLag/ViewModelLag.h"

namespace hooks {

SHookedFuncs gHooks = {};
engine_studio_api_t gEngineStudio = {};

static std::vector<hook_t*> s_aryEngineHook;
static std::vector<hook_t*> s_aryClientHook;

#define SIG_LENGTH(sig) (sizeof(sig) - 1)
#define FILL_SIG(sig, base, size, field) \
	(gHooks.field) = (decltype(gHooks.field))abc::SigScan((void*)(base), (size), sig, SIG_LENGTH(sig))
#define SIG_FUNC_NOT_FOUND(field) \
	if (!(gHooks.field)) abc::SysError("Could not find sig: %s\nEngine buildnum %d", #field, abc::g_dwEngineBuildnum)

#pragma region Engine hook bodies
static void H_R_NewMap() {
	abcess::moduleRegistry().newMapAll();
	gHooks.R_NewMap();
}

static model_t* H_CL_GetModelByIndex(int index) {
	model_t* pExtra = ExtraPrecache_GetByModelIndex(index);
	if (pExtra)
		return pExtra;
	return gHooks.CL_GetModelByIndex(index);
}

static void H_R_BloodSprite(float* org, int colorindex, int modelIndex, int modelIndex2, float size) {
	Efx::R_BloodSprite(org, colorindex, modelIndex, modelIndex2, size);
}

static TEMPENTITY* H_R_TempModel(float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype) {
	return Efx::R_TempModel(pos, dir, angles, life, modelIndex, soundtype);
}

static void H_pfnPlaybackEvent(int flags, const struct edict_s* pInvoker, unsigned short eventindex,
	float delay, float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2,
	int bparam1, int bparam2) {
	Efx::pfnPlaybackEvent(flags, pInvoker, eventindex, delay, origin, angles,
		fparam1, fparam2, iparam1, iparam2, bparam1, bparam2);
}

static void __fastcall H_CClient_SoundEngine_Initialize(void* pSoundEngine, int dummy) {
	gHooks.pClientSoundEngine = pSoundEngine;
	gHooks.CClient_SoundEngine_Initialize(pSoundEngine, dummy);
}
#pragma endregion

#pragma region Export-table thunks (chained)
static void RegisterGlobalCvars() {
	// read-only version stamp: any write snaps back (old PLUGIN_VERSION cvar)
	abc::CREATE_CVAR("abc_version", ABCESS_VERSION, FCVAR_EXTDLL | FCVAR_CLIENTDLL, [](cvar_t* cvar) {
		abc::CVAR_SET_STRING("abc_version", ABCESS_VERSION);
	});
}

static void H_HUD_Init() {
	static bool s_bInitialized = false;
	const bool first = !s_bInitialized;
	if (first) {
		s_bInitialized = true;
		// cvar/command registration happens at HUD_Init time (client ready),
		// the proven point from ABCEnchance (08 §4.9).
		RegisterGlobalCvars();
		abcess::moduleRegistry().initAll();
	}
	gExportfuncs.HUD_Init();
	// post-chain init: bindings that reference base-HUD commands (AutoFunc
	// replays persisted concurrent/event bindings after the base HUD has
	// registered its own commands - old code ordered it the same way).
	if (first)
		abcess::moduleRegistry().postInitAll();
}

static int H_HUD_VidInit() {
	abcess::moduleRegistry().vidInitAll();
	return gExportfuncs.HUD_VidInit();
}

static void H_HUD_Frame(double frametime) {
	gExportfuncs.HUD_Frame(frametime);
	// pump main-thread continuations (abc::Task / DispatchToMainThread)
	abc::GetTaskManager()->CheckAll();
	abcess::moduleRegistry().frameAll();
}

static void H_HUD_Shutdown() {
	abcess::moduleRegistry().shutdownAll();
	abcess::SaveConfig();
	gExportfuncs.HUD_Shutdown();
	UninstallClientHooks();
}

static int H_HUD_GetStudioModelInterface(int version, struct r_studio_interface_s** ppinterface, struct engine_studio_api_s* pstudio) {
	memcpy(&gEngineStudio, pstudio, sizeof(gEngineStudio));
	return gExportfuncs.HUD_GetStudioModelInterface(version, ppinterface, pstudio);
}

static int H_HUD_UpdateClientData(struct client_data_s* c, float f) {
	ViewModelLag::OnUpdateClientData(c);
	return gExportfuncs.HUD_UpdateClientData(c, f);
}

static void H_HUD_PlayerMove(struct playermove_s* ppmove, qboolean server) {
	gExportfuncs.HUD_PlayerMove(ppmove, server);
	AutoFunc::OnPlayerMove(ppmove->waterlevel > 1, ppmove->onground != -1, ppmove->movetype == MOVETYPE_WALK);
}

static void H_HUD_TxferPredictionData(struct entity_state_s* ps, const struct entity_state_s* pps,
	struct clientdata_s* pcd, const struct clientdata_s* ppcd, struct weapon_data_s* wd, const struct weapon_data_s* pwd) {
	AutoFunc::OnTxferPredictionData(wd);
	gExportfuncs.HUD_TxferPredictionData(ps, pps, pcd, ppcd, wd, pwd);
}

static void H_CL_CreateMove(float frametime, struct usercmd_s* cmd, int active) {
	gExportfuncs.CL_CreateMove(frametime, cmd, active);
	AutoFunc::AutoJump(cmd);
	AutoFunc::DuckTap(cmd);
}

static void H_V_CalcRefdef(struct ref_params_s* pparams) {
	// nextView is zeroed by the client dll - chain first (old order)
	gExportfuncs.V_CalcRefdef(pparams);
	if (!gExportfuncs.CL_IsThirdPerson()) {
		ViewModelLag::CalcRefdef(pparams);
	}
	else {
		Camera::CalcRefdef(pparams);
	}
}

static int H_HUD_AddEntity(int type, struct cl_entity_s* ent, const char* modelname) {
	if (Efx::AddEntity(type, ent))
		return 0;
	return gExportfuncs.HUD_AddEntity(type, ent, modelname);
}
#pragma endregion

#pragma region Fill / Install / Uninstall
void FillEngineAddress() {
	auto engineFactory = Sys_GetFactory((HINTERFACEMODULE)abc::g_dwEngineBase);
	if (engineFactory && engineFactory("EngineSurface007", nullptr)) {
#define R_NEWMAP_SIG "\x55\x8B\xEC\x51\xC7\x45\xFC\x00\x00\x00\x00\xEB\x2A\x8B\x45\xFC\x83\xC0\x01\x89\x45\xFC\x81\x7D\xFC\x00\x01\x00\x00"
		FILL_SIG(R_NEWMAP_SIG, abc::g_dwEngineBase, abc::g_dwEngineSize, R_NewMap);
		SIG_FUNC_NOT_FOUND(R_NewMap);
#define CL_FINDMODELBYINDEX_SIG "\x83\xEC\x08\x56\x57\x8B\x7C\x24\x14\x8B\x34\xBD\x2A\x2A\x2A\x2A\x85\xF6\x75\x08\x5F\x33\xC0\x5E\x83\xC4\x08\xC3"
		FILL_SIG(CL_FINDMODELBYINDEX_SIG, abc::g_dwEngineBase, abc::g_dwEngineSize, CL_GetModelByIndex);
		SIG_FUNC_NOT_FOUND(CL_GetModelByIndex);
	}
}

void InstallEngineHooks() {
	// engine efx API, detoured in place
	gHooks.R_BloodSprite = gEngfuncs.pEfxAPI->R_BloodSprite;
	s_aryEngineHook.push_back(g_pMetaHookAPI->InlineHook((void*)gHooks.R_BloodSprite, H_R_BloodSprite, (void**)&gHooks.R_BloodSprite));
	gHooks.R_TempModel = gEngfuncs.pEfxAPI->R_TempModel;
	s_aryEngineHook.push_back(g_pMetaHookAPI->InlineHook((void*)gHooks.R_TempModel, H_R_TempModel, (void**)&gHooks.R_TempModel));
	// engine engfunc, detoured
	gHooks.pfnPlaybackEvent = gEngfuncs.pfnPlaybackEvent;
	s_aryEngineHook.push_back(g_pMetaHookAPI->InlineHook((void*)gHooks.pfnPlaybackEvent, H_pfnPlaybackEvent, (void**)&gHooks.pfnPlaybackEvent));
	// engine sig-scanned, detoured
	s_aryEngineHook.push_back(g_pMetaHookAPI->InlineHook((void*)gHooks.R_NewMap, H_R_NewMap, (void**)&gHooks.R_NewMap));
	s_aryEngineHook.push_back(g_pMetaHookAPI->InlineHook((void*)gHooks.CL_GetModelByIndex, H_CL_GetModelByIndex, (void**)&gHooks.CL_GetModelByIndex));
}

void FillClientAddress() {
#define V_PUNCHAXIS_SIG "\x8B\x44\x24\x04\xF3\x0F\x10\x44\x24\x08\xF3\x0F\x11\x04\x85\x2A\x2A\x2A\x2A\xC3\xCC"
	FILL_SIG(V_PUNCHAXIS_SIG, abc::g_dwClientBase, abc::g_dwClientSize, V_PunchAxis);
	SIG_FUNC_NOT_FOUND(V_PunchAxis);
#define CLIENT_SOUNDENGINE_INITIALIZE_SIG "\x81\xEC\x8C\x00\x00\x00\xA1\x2A\x2A\x2A\x2A\x33\xC4\x89\x84\x24\x88\x00\x00\x00\x53\x57\x6A\x00"
	FILL_SIG(CLIENT_SOUNDENGINE_INITIALIZE_SIG, abc::g_dwClientBase, abc::g_dwClientSize, CClient_SoundEngine_Initialize);
	SIG_FUNC_NOT_FOUND(CClient_SoundEngine_Initialize);
#define CLIENT_SOUNDENGINE_PLAYFMODSOUND_SIG "\x55\x8B\xEC\x83\xE4\xF8\x81\xEC\x98\x00\x00\x00\xA1\x2A\x2A\x2A\x2A\x33"
	FILL_SIG(CLIENT_SOUNDENGINE_PLAYFMODSOUND_SIG, abc::g_dwClientBase, abc::g_dwClientSize, CClient_SoundEngine_PlayFMODSound);
	SIG_FUNC_NOT_FOUND(CClient_SoundEngine_PlayFMODSound);
}

void InstallClientHooks() {
	// CClient_SoundEngine_Initialize detour captures the FMOD wrapper "this";
	// PlayFMODSound stays unhooked (the old detour only hosted the NetEase
	// music branch, which is not migrated) and is called directly.
	s_aryClientHook.push_back(g_pMetaHookAPI->InlineHook((void*)gHooks.CClient_SoundEngine_Initialize,
		H_CClient_SoundEngine_Initialize, (void**)&gHooks.CClient_SoundEngine_Initialize));
}

void UninstallEngineHooks() {
	for (hook_t* h : s_aryEngineHook) {
		if (h)
			g_pMetaHookAPI->UnHook(h);
	}
	s_aryEngineHook.clear();
}

void UninstallClientHooks() {
	for (hook_t* h : s_aryClientHook) {
		if (h)
			g_pMetaHookAPI->UnHook(h);
	}
	s_aryClientHook.clear();
}
#pragma endregion

void AttachExports(cl_exportfuncs_t* pExportFuncs) {
	pExportFuncs->HUD_Init = H_HUD_Init;
	pExportFuncs->HUD_VidInit = H_HUD_VidInit;
	pExportFuncs->HUD_Frame = H_HUD_Frame;
	pExportFuncs->HUD_Shutdown = H_HUD_Shutdown;
	pExportFuncs->HUD_GetStudioModelInterface = H_HUD_GetStudioModelInterface;
	pExportFuncs->HUD_UpdateClientData = H_HUD_UpdateClientData;
	pExportFuncs->HUD_PlayerMove = H_HUD_PlayerMove;
	pExportFuncs->HUD_TxferPredictionData = H_HUD_TxferPredictionData;
	pExportFuncs->CL_CreateMove = H_CL_CreateMove;
	pExportFuncs->V_CalcRefdef = H_V_CalcRefdef;
	pExportFuncs->HUD_AddEntity = H_HUD_AddEntity;
}

}
