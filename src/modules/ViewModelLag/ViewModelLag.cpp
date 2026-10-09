// Viewmodel lag / slide + first-person view offset.
// Ported from ABCEnchance src/core/module/viewmodellag.* (GBK -> UTF-8); the
// SourceSDK Vector-class expressions are rewritten over abc::CMathlib array
// math - AbcEssential carries no vector class by design.
// Lag logic copied and edited from
// https://github.com/SNMetamorph/PrimeXT/blob/master/client/r_view.cpp
#include "ViewModelLag.h"

#include <algorithm>

#include <metahook.h>

#include <cdll_int.h>
#include <event_api.h>
#include <pm_defs.h>
#include <pmtrace.h>
#include <ref_params.h>

#include <abc/CvarCmd.h>
#include <abc/Math.h>

namespace {

using abc::CMathlib;

cvar_t* s_pModelLag;
cvar_t* s_pModelLagAutoStop;
cvar_t* s_pModelLagValue;
cvar_t* s_pModelSlide;
cvar_t* s_pModelSlideAngle;
cvar_t* s_pModelSlideLength;
cvar_t* s_pModelSlideHeight;

cvar_t* s_pDefaultFov = nullptr; // engine cvar, grabbed at VidInit
float s_flFov = 90.0f;           // current fov, fed from HUD_UpdateClientData

void CalcViewModelLag(ref_params_t* pparams, vec3_t origin, vec3_t angles, const vec3_t original_angles) {
	if (s_pModelLag->value <= 0)
		return;
	// autostop: hold the viewmodel still while the fov differs from default (zoom)
	if (s_pModelLagAutoStop->value > 0 && s_pDefaultFov &&
		s_flFov != std::clamp<float>(s_pDefaultFov->value, 10.0f, 110.0f))
		return;
	static vec3_t s_vecLastFacing = { 0, 0, 0 };
	vec3_t vOriginalOrigin, vOriginalAngles;
	CMathlib::VectorCopy(origin, vOriginalOrigin);
	CMathlib::VectorCopy(angles, vOriginalAngles);

	// Calculate our drift
	vec3_t forward, right, up;
	CMathlib::AngleVectors(angles, forward, right, up);

	if (pparams->frametime != 0.0f) { // not paused
		vec3_t vDifference;
		CMathlib::VectorSubtract(forward, s_vecLastFacing, vDifference);

		float flSpeed = 5.0f;
		// If we start to lag too far behind, increase the "catch up" speed.
		// Solves the problem with fast cl_yawspeed, m_yaw or joysticks rotating
		// quickly; the old code would slam lastfacing with origin causing the
		// viewmodel to pop to a new position.
		float flDiff = CMathlib::FVectorLength(vDifference);
		if ((flDiff > s_pModelLagValue->value) && (s_pModelLagValue->value > 0.0f)) {
			float flScale = flDiff / s_pModelLagValue->value;
			flSpeed *= flScale;
		}

		// FIXME:  Needs to be predictable?
		// s_vecLastFacing += vDifference * (flSpeed * frametime)
		CMathlib::VectorMA(s_vecLastFacing, flSpeed * pparams->frametime, vDifference, s_vecLastFacing);
		// Make sure it doesn't grow out of control
		CMathlib::VectorNormalize(s_vecLastFacing);
		// origin += vDifference * -flSpeed
		vec3_t vScaled;
		CMathlib::VectorScale(vDifference, -1.0f * flSpeed, vScaled);
		CMathlib::VectorAdd(origin, vScaled, origin);
	}

	CMathlib::AngleVectors(original_angles, forward, right, up);
	float pitch = original_angles[CMathlib::Q_PITCH];
	if (pitch > 180.0f)
		pitch -= 360.0f;
	else if (pitch < -180.0f)
		pitch += 360.0f;

	if (s_pModelLagValue->value <= 0.0f) {
		CMathlib::VectorCopy(vOriginalOrigin, origin);
		CMathlib::VectorCopy(vOriginalAngles, angles);
	}
	else {
		// FIXME: These are the old settings that caused too many exposed polys on some models
		CMathlib::VectorMA(origin, -pitch * 0.035f, forward, origin);
		CMathlib::VectorMA(origin, -pitch * 0.03f, right, origin);
		CMathlib::VectorMA(origin, -pitch * 0.02f, up, origin);
	}
}

void CalcModelSlide(ref_params_t* pparams) {
	if (s_pModelSlide->value <= 0)
		return;
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	pmtrace_t tr;
	vec3_t vecEnd;
	CMathlib::VectorCopy(local->curstate.origin, vecEnd);
	vecEnd[2] += -4;
	gEngfuncs.pEventAPI->EV_SetTraceHull(0);
	gEngfuncs.pEventAPI->EV_PlayerTrace(local->curstate.origin, vecEnd, PM_STUDIO_IGNORE, local->index, &tr);
	if (tr.fraction >= 1)
		return;
	float flViewHeight = std::clamp<float>(pparams->viewheight[2], 12.0f, 28.0f);
	//12 Duck
	//28 Stand
	float flSlideRatio = 1 - ((flViewHeight - 12) / 16);
	cl_entity_t* view = gEngfuncs.GetViewModel();
	vec3_t vecRight;
	CMathlib::AngleVectors(view->angles, nullptr, vecRight, nullptr);
	CMathlib::VectorNormalize(vecRight);
	CMathlib::VectorScale(vecRight, -s_pModelSlideLength->value * flSlideRatio, vecRight);
	view->curstate.angles[2] = -s_pModelSlideAngle->value * flSlideRatio;
	CMathlib::VectorAdd(view->origin, vecRight, view->origin);
	view->origin[2] -= s_pModelSlideHeight->value * flSlideRatio;
}

}

void ViewModelLag::Init() {
	s_pModelLag = abc::CREATE_CVAR("cl_modellag", "1", abc::FCVAR_VALUE, nullptr);
	s_pModelLagAutoStop = abc::CREATE_CVAR("cl_modellag_autostop", "1", abc::FCVAR_VALUE, nullptr);
	s_pModelLagValue = abc::CREATE_CVAR("cl_modellag_value", "1.0", abc::FCVAR_VALUE, nullptr);

	s_pModelSlide = abc::CREATE_CVAR("cl_modelslide", "0", abc::FCVAR_VALUE, nullptr);
	s_pModelSlideAngle = abc::CREATE_CVAR("cl_modelslide_angle", "24", abc::FCVAR_VALUE, nullptr);
	s_pModelSlideLength = abc::CREATE_CVAR("cl_modelslide_length", "12", abc::FCVAR_VALUE, nullptr);
	s_pModelSlideHeight = abc::CREATE_CVAR("cl_modelslide_height", "12", abc::FCVAR_VALUE, nullptr);
}

void ViewModelLag::VidInit() {
	s_pDefaultFov = abc::CVAR_GET_POINTER("default_fov");
}

void ViewModelLag::OnUpdateClientData(const struct client_data_s* c) {
	s_flFov = c->fov;
}

void ViewModelLag::CalcRefdef(ref_params_t* pparams) {
	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV (old hub behavior)
	cl_entity_t* view = gEngfuncs.GetViewModel();
	if (pparams->viewsize == 110)
		view->origin[2] += 1;
	else if (pparams->viewsize == 100)
		view->origin[2] += 2;
	else if (pparams->viewsize == 90)
		view->origin[2] += 1;
	else if (pparams->viewsize == 80)
		view->origin[2] += 0.5;

	vec3_t viewOrigin, viewAngles;
	CMathlib::VectorCopy(view->origin, viewOrigin);
	CMathlib::VectorCopy(view->angles, viewAngles);
	CalcViewModelLag(pparams, viewOrigin, viewAngles, pparams->cl_viewangles);
	CMathlib::VectorCopy(viewOrigin, view->origin);
	CMathlib::VectorCopy(viewAngles, view->angles);
	CalcModelSlide(pparams);
}
