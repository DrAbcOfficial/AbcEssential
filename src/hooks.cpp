// AbcEssential hook installation and chained export thunks (plan 04 §1).
// Ported from ABCEnchance src/exportfuncs.cpp; module-specific hook bodies
// are added by the feature modules (modules/) into the seams below.
// The spine owns every detoured original: hook bodies delegate to feature
// modules and always chain to the original themselves (R-2).
#include "hooks.h"

#include <cstring>
#include <vector>

#include <pm_defs.h>

#include <abc/Console.h>
#include <abc/CvarCmd.h>
#include <abc/Plugin.h>
#include <abc/Task.h>

#include "modules/Module.h"
#include "modules/autofunc/AutoFunc.h"
#include "modules/autofunc/Signals.h"
#include "modules/camera/Camera.h"
#include "modules/fx/Efx.h"
#include "modules/viewmodellag/ViewModelLag.h"
#include "services/Config.h"
#include "services/ExtraPrecache.h"
#include "services/Version.h"

namespace hooks {

HookedFuncs Hooks = {};
engine_studio_api_t EngineStudio = {};

static std::vector<hook_t*> EngineHookList;
static std::vector<hook_t*> ClientHookList;

#define SIG_LENGTH(sig) (sizeof(sig) - 1)
#define FILL_SIG(sig, base, size, field) \
	(Hooks.field) = (decltype(Hooks.field))abc::SigScan((void*)(base), (size), sig, SIG_LENGTH(sig))
#define SIG_FUNC_NOT_FOUND(field) \
	if (!(Hooks.field)) abc::SysError("Could not find sig: %s\nEngine buildnum %d", #field, abc::g_dwEngineBuildnum)

#pragma region Engine hook bodies
static void H_R_NewMap() {
	abcess::moduleRegistry().newMapAll();
	Hooks.R_NewMap();
}

static model_t* H_CL_GetModelByIndex(int index) {
	model_t* pExtra = extraprecache::getByModelIndex(index);
	if (pExtra)
		return pExtra;
	return Hooks.CL_GetModelByIndex(index);
}

static void H_R_BloodSprite(float* org, int colorindex, int modelIndex, int modelIndex2, float size) {
	fx::bloodSprite(org, colorindex, modelIndex, modelIndex2, size);
	// the chain back to the original is always ours (R-2) - also when the
	// enhancement pass bailed out early
	Hooks.R_BloodSprite(org, colorindex, modelIndex, modelIndex2, size);
}

static TEMPENTITY* H_R_TempModel(float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype) {
	// efx needs the original entry itself (smoke shell spawns through it),
	// injected as a parameter - modules never read the chain fields
	return fx::tempModel(pos, dir, angles, life, modelIndex, soundtype, Hooks.R_TempModel);
}

static void H_pfnPlaybackEvent(int flags, const struct edict_s* pInvoker, unsigned short eventindex,
	float delay, float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2,
	int bparam1, int bparam2) {
	if (fx::playbackEvent(flags, pInvoker, eventindex, delay, origin, angles,
		fparam1, fparam2, iparam1, iparam2, bparam1, bparam2))
		return;
	Hooks.pfnPlaybackEvent(flags, pInvoker, eventindex, delay, origin, angles,
		fparam1, fparam2, iparam1, iparam2, bparam1, bparam2);
}

static void __fastcall H_CClient_SoundEngine_Initialize(void* pSoundEngine, int dummy) {
	Hooks.ClientSoundEngine = pSoundEngine;
	Hooks.CClient_SoundEngine_Initialize(pSoundEngine, dummy);
}
#pragma endregion

#pragma region Export-table thunks (chained)
static void registerGlobalCvars() {
	// read-only version stamp: any write snaps back (old PLUGIN_VERSION cvar)
	abc::CREATE_CVAR("abc_version", abcess::kVersion, FCVAR_EXTDLL | FCVAR_CLIENTDLL, [](cvar_t* cvar) {
		abc::CVAR_SET_STRING("abc_version", abcess::kVersion);
	});
}

static void H_HUD_Init() {
	static bool s_bInitialized = false;
	const bool first = !s_bInitialized;
	if (first) {
		s_bInitialized = true;
		// cvar/command registration happens at HUD_Init time (client ready),
		// the proven point from ABCEnchance (08 §4.9).
		registerGlobalCvars();
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
	abcess::saveConfig();
	gExportfuncs.HUD_Shutdown();
	uninstallClientHooks();
}

static int H_HUD_GetStudioModelInterface(int version, struct r_studio_interface_s** ppinterface, struct engine_studio_api_s* pstudio) {
	memcpy(&EngineStudio, pstudio, sizeof(EngineStudio));
	extraprecache::setEngineStudio(pstudio);
	return gExportfuncs.HUD_GetStudioModelInterface(version, ppinterface, pstudio);
}

static int H_HUD_UpdateClientData(struct client_data_s* c, float f) {
	viewmodellag::onUpdateClientData(c);
	return gExportfuncs.HUD_UpdateClientData(c, f);
}

static void H_HUD_PlayerMove(struct playermove_s* ppmove, qboolean server) {
	gExportfuncs.HUD_PlayerMove(ppmove, server);
	autofunc::onPlayerMove(ppmove->waterlevel > 1, ppmove->onground != -1, ppmove->movetype == MOVETYPE_WALK);
}

static void H_HUD_TxferPredictionData(struct entity_state_s* ps, const struct entity_state_s* pps,
	struct clientdata_s* pcd, const struct clientdata_s* ppcd, struct weapon_data_s* wd, const struct weapon_data_s* pwd) {
	signals::onTxferPredictionData(wd);
	gExportfuncs.HUD_TxferPredictionData(ps, pps, pcd, ppcd, wd, pwd);
}

static void H_CL_CreateMove(float frametime, struct usercmd_s* cmd, int active) {
	gExportfuncs.CL_CreateMove(frametime, cmd, active);
	autofunc::autoJump(cmd);
	autofunc::duckTap(cmd);
}

static void H_V_CalcRefdef(struct ref_params_s* pparams) {
	// nextView is zeroed by the client dll - chain first (old order)
	gExportfuncs.V_CalcRefdef(pparams);
	if (!gExportfuncs.CL_IsThirdPerson())
		viewmodellag::calcRefdef(pparams);
	else
		camera::calcRefdef(pparams);
}

static int H_HUD_AddEntity(int type, struct cl_entity_s* ent, const char* modelname) {
	if (fx::addEntity(type, ent))
		return 0;
	return gExportfuncs.HUD_AddEntity(type, ent, modelname);
}
#pragma endregion

#pragma region Fill / Install / Uninstall
void fillEngineAddress() {
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

void installEngineHooks() {
	// engine efx API, detoured in place
	Hooks.R_BloodSprite = gEngfuncs.pEfxAPI->R_BloodSprite;
	EngineHookList.push_back(g_pMetaHookAPI->InlineHook((void*)Hooks.R_BloodSprite, H_R_BloodSprite, (void**)&Hooks.R_BloodSprite));
	Hooks.R_TempModel = gEngfuncs.pEfxAPI->R_TempModel;
	EngineHookList.push_back(g_pMetaHookAPI->InlineHook((void*)Hooks.R_TempModel, H_R_TempModel, (void**)&Hooks.R_TempModel));
	// engine engfunc, detoured
	Hooks.pfnPlaybackEvent = gEngfuncs.pfnPlaybackEvent;
	EngineHookList.push_back(g_pMetaHookAPI->InlineHook((void*)Hooks.pfnPlaybackEvent, H_pfnPlaybackEvent, (void**)&Hooks.pfnPlaybackEvent));
	// engine sig-scanned, detoured
	EngineHookList.push_back(g_pMetaHookAPI->InlineHook((void*)Hooks.R_NewMap, H_R_NewMap, (void**)&Hooks.R_NewMap));
	EngineHookList.push_back(g_pMetaHookAPI->InlineHook((void*)Hooks.CL_GetModelByIndex, H_CL_GetModelByIndex, (void**)&Hooks.CL_GetModelByIndex));
}

void fillClientAddress() {
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

void installClientHooks() {
	// CClient_SoundEngine_Initialize detour captures the FMOD wrapper "this";
	// PlayFMODSound stays unhooked (the old detour only hosted the NetEase
	// music branch, which is not migrated) and is called directly.
	ClientHookList.push_back(g_pMetaHookAPI->InlineHook((void*)Hooks.CClient_SoundEngine_Initialize,
		H_CClient_SoundEngine_Initialize, (void**)&Hooks.CClient_SoundEngine_Initialize));
}

void uninstallEngineHooks() {
	for (hook_t* h : EngineHookList) {
		if (h)
			g_pMetaHookAPI->UnHook(h);
	}
	EngineHookList.clear();
}

void uninstallClientHooks() {
	for (hook_t* h : ClientHookList) {
		if (h)
			g_pMetaHookAPI->UnHook(h);
	}
	ClientHookList.clear();
}
#pragma endregion

void attachExports(cl_exportfuncs_t* pExportFuncs) {
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
