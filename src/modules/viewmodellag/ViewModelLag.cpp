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

#include "../Module.h"

namespace viewmodellag {

namespace {

using abc::CMathlib;

cvar_t *ModelLagCvar;
cvar_t *ModelLagAutoStopCvar;
cvar_t *ModelLagValueCvar;
cvar_t *ModelSlideCvar;
cvar_t *ModelSlideAngleCvar;
cvar_t *ModelSlideLengthCvar;
cvar_t *ModelSlideHeightCvar;

cvar_t *DefaultFovCvar = nullptr; // engine cvar, grabbed at VidInit
float CurrentFov = 90.0f;         // current fov, fed from HUD_UpdateClientData

void calcViewModelLag(ref_params_t *pparams, vec3_t origin, vec3_t angles,
                      const vec3_t originalAngles) {
  if (ModelLagCvar->value <= 0)
    return;
  // autostop: hold the viewmodel still while the fov differs from default
  // (zoom)
  if (ModelLagAutoStopCvar->value > 0 && DefaultFovCvar &&
      CurrentFov != std::clamp<float>(DefaultFovCvar->value, 10.0f, 110.0f))
    return;
  static vec3_t LastFacing = {0, 0, 0};
  vec3_t OriginalOrigin, OriginalAngles;
  CMathlib::VectorCopy(origin, OriginalOrigin);
  CMathlib::VectorCopy(angles, OriginalAngles);

  // Calculate our drift
  vec3_t forward, right, up;
  CMathlib::AngleVectors(angles, forward, right, up);

  if (pparams->frametime != 0.0f) { // not paused
    vec3_t difference;
    CMathlib::VectorSubtract(forward, LastFacing, difference);

    float speed = 5.0f;
    // If we start to lag too far behind, increase the "catch up" speed.
    // Solves the problem with fast cl_yawspeed, m_yaw or joysticks rotating
    // quickly; the old code would slam lastfacing with origin causing the
    // viewmodel to pop to a new position.
    float diff = CMathlib::FVectorLength(difference);
    if ((diff > ModelLagValueCvar->value) &&
        (ModelLagValueCvar->value > 0.0f)) {
      float scale = diff / ModelLagValueCvar->value;
      speed *= scale;
    }

    // FIXME:  Needs to be predictable?
    // LastFacing += difference * (speed * frametime)
    CMathlib::VectorMA(LastFacing, speed * pparams->frametime, difference,
                       LastFacing);
    // Make sure it doesn't grow out of control
    CMathlib::VectorNormalize(LastFacing);
    // origin += difference * -speed
    vec3_t scaled;
    CMathlib::VectorScale(difference, -1.0f * speed, scaled);
    CMathlib::VectorAdd(origin, scaled, origin);
  }

  CMathlib::AngleVectors(originalAngles, forward, right, up);
  float pitch = originalAngles[CMathlib::Q_PITCH];
  if (pitch > 180.0f)
    pitch -= 360.0f;
  else if (pitch < -180.0f)
    pitch += 360.0f;

  if (ModelLagValueCvar->value <= 0.0f) {
    CMathlib::VectorCopy(OriginalOrigin, origin);
    CMathlib::VectorCopy(OriginalAngles, angles);
  } else {
    // FIXME: These are the old settings that caused too many exposed polys on
    // some models
    CMathlib::VectorMA(origin, -pitch * 0.035f, forward, origin);
    CMathlib::VectorMA(origin, -pitch * 0.03f, right, origin);
    CMathlib::VectorMA(origin, -pitch * 0.02f, up, origin);
  }
}

void calcModelSlide(ref_params_t *pparams) {
  if (ModelSlideCvar->value <= 0)
    return;
  cl_entity_t *local = gEngfuncs.GetLocalPlayer();
  pmtrace_t tr;
  vec3_t vecEnd;
  CMathlib::VectorCopy(local->curstate.origin, vecEnd);
  vecEnd[2] += -4;
  gEngfuncs.pEventAPI->EV_SetTraceHull(0);
  gEngfuncs.pEventAPI->EV_PlayerTrace(local->curstate.origin, vecEnd,
                                      PM_STUDIO_IGNORE, local->index, &tr);
  if (tr.fraction >= 1)
    return;
  // 12 Duck
  // 28 Stand
  float viewHeight = std::clamp<float>(pparams->viewheight[2], 12.0f, 28.0f);
  float slideRatio = 1 - ((viewHeight - 12) / 16);
  cl_entity_t *view = gEngfuncs.GetViewModel();
  vec3_t right;
  CMathlib::AngleVectors(view->angles, nullptr, right, nullptr);
  CMathlib::VectorNormalize(right);
  CMathlib::VectorScale(right, -ModelSlideLengthCvar->value * slideRatio,
                        right);
  view->curstate.angles[2] = -ModelSlideAngleCvar->value * slideRatio;
  CMathlib::VectorAdd(view->origin, right, view->origin);
  view->origin[2] -= ModelSlideHeightCvar->value * slideRatio;
}

} // namespace

void init() {
  ModelLagCvar =
      abc::CREATE_CVAR("cl_modellag", "1", abc::FCVAR_VALUE, nullptr);
  ModelLagAutoStopCvar =
      abc::CREATE_CVAR("cl_modellag_autostop", "1", abc::FCVAR_VALUE, nullptr);
  ModelLagValueCvar =
      abc::CREATE_CVAR("cl_modellag_value", "1.0", abc::FCVAR_VALUE, nullptr);

  ModelSlideCvar =
      abc::CREATE_CVAR("cl_modelslide", "0", abc::FCVAR_VALUE, nullptr);
  ModelSlideAngleCvar =
      abc::CREATE_CVAR("cl_modelslide_angle", "24", abc::FCVAR_VALUE, nullptr);
  ModelSlideLengthCvar =
      abc::CREATE_CVAR("cl_modelslide_length", "12", abc::FCVAR_VALUE, nullptr);
  ModelSlideHeightCvar =
      abc::CREATE_CVAR("cl_modelslide_height", "12", abc::FCVAR_VALUE, nullptr);
}

void vidInit() {
  DefaultFovCvar = abc::CVAR_GET_POINTER("default_fov");
}

void onUpdateClientData(const struct client_data_s *c) {
  CurrentFov = c->fov;
}

void calcRefdef(ref_params_t *pparams) {
  // fudge position around to keep amount of weapon visible
  // roughly equal with different FOV (old hub behavior)
  cl_entity_t *view = gEngfuncs.GetViewModel();
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
  calcViewModelLag(pparams, viewOrigin, viewAngles, pparams->cl_viewangles);
  CMathlib::VectorCopy(viewOrigin, view->origin);
  CMathlib::VectorCopy(viewAngles, view->angles);
  calcModelSlide(pparams);
}

namespace {
class ViewModelLagModule final : public abcess::IFeatureModule {
public:
  ViewModelLagModule() {
    abcess::moduleRegistry().add(this);
  }
  const char *name() const override {
    return "viewmodellag";
  }
  void onInit() override {
    init();
  }
  void onVidInit() override {
    vidInit();
  }
};
const ViewModelLagModule RegisterModule;
} // namespace

} // namespace viewmodellag
