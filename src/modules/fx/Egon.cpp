// Egon particle ball: enemy beam recognition + temp-entity particle stream.
// Ported from ABCEnchance src/core/module/efxenchance.cpp egon parts.
// Deviation (plan 04 §2): the old build read the view trace from the
// playertrace module (now AbcUi's data layer); the trace runs directly here.

#include "EfxInternal.h"

#include <cstring>

#include <metahook.h>

#include <cl_entity.h>
#include <com_model.h>
#include <const.h>
#include <entity_types.h>
#include <event_api.h>
#include <pm_defs.h>
#include <pmtrace.h>
#include <r_efx.h>

#include <abc/EngineRandom.h>
#include <abc/Math.h>

namespace fx {

namespace math = abc::math;

void shootEgonParticle() {
  model_t *ballModel = gEngfuncs.hudGetModelByIndex(Precached.EgonBeam);
  if (!ballModel)
    return;
  cl_entity_t *local = gEngfuncs.GetLocalPlayer();
  if (!local)
    return;
  vec3_t src;
  math::VectorCopy(local->attachment[0], src);
  TEMPENTITY *ball = gEngfuncs.pEfxAPI->CL_TempEntAlloc(src, ballModel);
  if (!ball)
    return;
  // old build read the view trace from the playertrace module (now
  // AbcUi's data layer); trace straight ahead instead
  vec3_t viewAngles, forward, start, end, viewOfs;
  gEngfuncs.GetViewAngles(viewAngles);
  math::AngleVectors(viewAngles, forward, nullptr, nullptr);
  gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(viewOfs);
  math::VectorAdd(local->origin, viewOfs, start);
  math::VectorMA(start, 4096.0f, forward, end);
  gEngfuncs.pEventAPI->EV_SetTraceHull(2);
  pmtrace_t trace;
  gEngfuncs.pEventAPI->EV_PlayerTrace(start, end, PM_STUDIO_BOX, -1, &trace);

  float speed = 4096;
  vec3_t toEnd, velocity;
  math::VectorCopy(trace.endpos, toEnd);
  math::VectorSubtract(toEnd, src, toEnd);
  math::VectorCopy(toEnd, velocity);
  math::VectorNormalize(velocity);
  math::VectorScale(velocity, speed, velocity);
  math::VectorCopy(velocity, ball->entity.baseline.origin);
  ball->flags = FTENT_SPRANIMATELOOP | FTENT_CLIENTCUSTOM;
  ball->clientIndex = static_cast<short>(gEngfuncs.GetLocalPlayer()->index);
  ball->bounceFactor = 0;
  ball->entity.baseline.renderamt = 80;
  ball->entity.curstate.solid = SOLID_NOT;
  ball->entity.curstate.owner =
      static_cast<short>(gEngfuncs.GetLocalPlayer()->index);
  ball->entity.curstate.scale = math::RANDOM_FLOAT(0.2f, 0.4f);
  ball->entity.curstate.renderamt = 255;
  ball->entity.curstate.rendercolor = EgonParticleColor;
  ball->entity.curstate.rendermode = kRenderTransAdd;
  ball->entity.curstate.frame = 0;
  ball->entity.curstate.framerate = 10.0f * ballModel->numframes; // 1s
  ball->callback = [](struct tempent_s *ent, float /*frametime*/,
                      float currenttime) {
    if (currenttime >= ent->entity.curstate.fuser1) {
      ent->die = 0;
      TEMPENTITY *burst = gEngfuncs.pEfxAPI->CL_TempEntAlloc(
          ent->entity.prevstate.origin, ent->entity.model);
      if (!burst)
        return;
      math::VectorCopy(ent->entity.baseline.origin,
                       burst->entity.baseline.origin);
      burst->flags = FTENT_SPRANIMATELOOP | FTENT_COLLIDEWORLD | FTENT_FADEOUT |
                     FTENT_CLIENTCUSTOM;
      burst->clientIndex = ent->clientIndex;
      burst->bounceFactor = ent->bounceFactor;
      burst->entity.baseline.renderamt = ent->entity.baseline.renderamt;
      burst->entity.curstate.scale = ent->entity.curstate.scale;
      burst->entity.curstate.renderamt = ent->entity.curstate.renderamt;
      burst->entity.curstate.rendercolor = ent->entity.curstate.rendercolor;
      burst->entity.curstate.rendermode = ent->entity.curstate.rendermode;
      burst->entity.curstate.frame = ent->entity.curstate.frame;
      burst->entity.curstate.framerate = ent->entity.curstate.framerate;
      burst->entity.curstate.owner = ent->entity.curstate.owner;
      burst->hitcallback = [](struct tempent_s *hit,
                              struct pmtrace_s *hitTrace) {
        if (hit->entity.curstate.iuser1 != 1) {
          gEngfuncs.pEfxAPI->R_DecalShoot(
              gEngfuncs.pEfxAPI->Draw_DecalIndex(
                  gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(
                      const_cast<char *>("{smscorch2"))),
              gEngfuncs.pEventAPI->EV_IndexFromTrace(hitTrace), 0,
              hitTrace->endpos, 0);
          hit->entity.curstate.iuser1 = 1;
        }
      };
      burst->callback = [](struct tempent_s *dying, float /*frametime*/,
                           float currenttime) {
        if (currenttime >= dying->die) {
          dying->entity.baseline.origin[0] = 0;
          dying->entity.baseline.origin[1] = 0;
          dying->entity.baseline.origin[2] = 0;
        }
      };
      burst->die = static_cast<float>(currenttime) + 0.007f;
    }
  };
  ball->entity.curstate.fuser1 =
      static_cast<float>(gEngfuncs.GetClientTime()) +
      static_cast<float>(math::FVectorLength(toEnd)) / speed - 0.007f;
  EgonNextShoot = gEngfuncs.GetClientTime() + 0.007f;
}

bool addEntity(int type, struct cl_entity_s *ent) {
  if (!EgonEfxCvar || EgonEfxCvar->value <= 0 || !ent || Precached.EgonBeam < 0)
    return false;
  model_t *Model = ent->curstate.modelindex > 0
                       ? gEngfuncs.hudGetModelByIndex(ent->curstate.modelindex)
                       : nullptr;
  if (!Model)
    return false;
  if (type == ET_BEAM) {
    int beamType = ent->curstate.rendermode & 0x0F;
    if (beamType == 1) {
      int startEntity = ent->curstate.sequence;
      int endEntity = ent->curstate.skin;
      int entityType = ent->curstate.entityType;
      if (startEntity == 0 && endEntity > 4096 && entityType == 2 &&
          !strncmp(Model->name, "sprites/xbeam1.spr", 19)) {
        int beamFlags = (ent->curstate.rendermode & 0xF0);
        int noise = ent->curstate.body;
        float scrollRate = ent->curstate.animtime;
        if ((beamFlags == 16 && scrollRate == 5.0f && noise == 20) ||
            (beamFlags == 0 && scrollRate == 2.5f && noise == 8)) {
          int index = endEntity & 0xFFF;
          if (index <= 33 && index >= 1) {
            cl_entity_t *end = gEngfuncs.GetEntityByIndex(index);
            if (end && end == gEngfuncs.GetLocalPlayer()) {
              EgonParticleColor = ent->curstate.rendercolor;
              EgonMessageNum = ent->curstate.messagenum;
              EgonShooter = true;
              ent->curstate.rendercolor = {0, 0, 0};
            }
          }
        }
      }
    }
  } else if (type == ET_NORMAL) {
    // filter the vanilla egon spark sprites, the particle ball replaces them
    if (ent->curstate.modelindex > 0 &&
        !strncmp(Model->name, "sprites/XSpark1.spr", 19) &&
        ent->curstate.entityType == 1 && ent->curstate.scale == 1.0f &&
        ent->curstate.rendermode == 3 && ent->curstate.renderamt == 255 &&
        ent->curstate.rendercolor.r == 255 &&
        ent->curstate.rendercolor.g == 255 &&
        ent->curstate.rendercolor.b == 255 && ent->curstate.renderfx == 14 &&
        ent->curstate.movetype == 8) {
      return true;
    }
  }
  return false;
}

} // namespace fx
