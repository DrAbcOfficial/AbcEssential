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

#include <abc/Math.h>

namespace Efx {

using abc::CMathlib;

void ShootEgonParticle() {
	model_t* pModel = gEngfuncs.hudGetModelByIndex(s_Precache.iEgonBeam);
	if (!pModel)
		return;
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	if (!local)
		return;
	vec3_t vecSrc;
	CMathlib::VectorCopy(local->attachment[0], vecSrc);
	TEMPENTITY* tent = gEngfuncs.pEfxAPI->CL_TempEntAlloc(vecSrc, pModel);
	if (tent) {
		// old build read the view trace from the playertrace module (now
		// AbcUi's data layer); trace straight ahead instead
		vec3_t vecViewAngles, vecForward, vecStart, vecEnd, viewOfs;
		gEngfuncs.GetViewAngles(vecViewAngles);
		CMathlib::AngleVectors(vecViewAngles, vecForward, nullptr, nullptr);
		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(viewOfs);
		CMathlib::VectorAdd(local->origin, viewOfs, vecStart);
		CMathlib::VectorMA(vecStart, 4096.0f, vecForward, vecEnd);
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, PM_STUDIO_BOX, -1, &tr);

		vec3_t vecLength, vecVelocity;
		CMathlib::VectorCopy(tr.endpos, vecLength);
		CMathlib::VectorSubtract(vecLength, vecSrc, vecLength);
		float flSpeed = 4096;
		CMathlib::VectorCopy(vecLength, vecVelocity);
		CMathlib::VectorNormalize(vecVelocity);
		CMathlib::VectorScale(vecVelocity, flSpeed, vecVelocity);
		CMathlib::VectorCopy(vecVelocity, tent->entity.baseline.origin);
		tent->flags = FTENT_SPRANIMATELOOP | FTENT_CLIENTCUSTOM;
		tent->clientIndex = gEngfuncs.GetLocalPlayer()->index;
		tent->bounceFactor = 0;
		tent->entity.baseline.renderamt = 80;
		tent->entity.curstate.solid = SOLID_NOT;
		tent->entity.curstate.owner = gEngfuncs.GetLocalPlayer()->index;
		tent->entity.curstate.scale = CMathlib::RANDOM_FLOAT(0.2f, 0.4f);
		tent->entity.curstate.renderamt = 255;
		tent->entity.curstate.rendercolor = s_egonParticleColor;
		tent->entity.curstate.rendermode = kRenderTransAdd;
		tent->entity.curstate.frame = 0;
		tent->entity.curstate.framerate = 10.0f * pModel->numframes; //1s
		tent->callback = [](struct tempent_s* ent, float frametime, float currenttime) {
			if (currenttime >= ent->entity.curstate.fuser1) {
				ent->die = 0;
				TEMPENTITY* tent = gEngfuncs.pEfxAPI->CL_TempEntAlloc(ent->entity.prevstate.origin, ent->entity.model);
				if (!tent)
					return;
				CMathlib::VectorCopy(ent->entity.baseline.origin, tent->entity.baseline.origin);
				tent->flags = FTENT_SPRANIMATELOOP | FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_CLIENTCUSTOM;
				tent->clientIndex = ent->clientIndex;
				tent->bounceFactor = ent->bounceFactor;
				tent->entity.baseline.renderamt = ent->entity.baseline.renderamt;
				tent->entity.curstate.scale = ent->entity.curstate.scale;
				tent->entity.curstate.renderamt = ent->entity.curstate.renderamt;
				tent->entity.curstate.rendercolor = ent->entity.curstate.rendercolor;
				tent->entity.curstate.rendermode = ent->entity.curstate.rendermode;
				tent->entity.curstate.frame = ent->entity.curstate.frame;
				tent->entity.curstate.framerate = ent->entity.curstate.framerate;
				tent->entity.curstate.owner = ent->entity.curstate.owner;
				tent->hitcallback = [](struct tempent_s* ent, struct pmtrace_s* ptr) {
					if (ent->entity.curstate.iuser1 != 1) {
						gEngfuncs.pEfxAPI->R_DecalShoot(
							gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName(const_cast<char*>("{smscorch2"))),
							gEngfuncs.pEventAPI->EV_IndexFromTrace(ptr), 0, ptr->endpos, 0);
						ent->entity.curstate.iuser1 = 1;
					}
				};
				tent->callback = [](struct tempent_s* ent, float frametime, float currenttime) {
					if (currenttime >= ent->die) {
						ent->entity.baseline.origin[0] = 0;
						ent->entity.baseline.origin[1] = 0;
						ent->entity.baseline.origin[2] = 0;
					}
				};
				tent->die = currenttime + 0.007;
			}
		};
		tent->entity.curstate.fuser1 = gEngfuncs.GetClientTime() + CMathlib::FVectorLength(vecLength) / flSpeed - 0.007f;
		s_egonNextShoot = gEngfuncs.GetClientTime() + 0.007f;
	}
}

bool AddEntity(int type, struct cl_entity_s* ent) {
	if (s_pEgonEfx->value <= 0)
		return false;
	if (type == ET_BEAM) {
		int beamtype = ent->curstate.rendermode & 0x0F;
		if (beamtype == 1) {
			int startent = ent->curstate.sequence;
			int endent = ent->curstate.skin;
			int etype = ent->curstate.entityType;
			if (startent == 0 && endent > 4096 && etype == 2 &&
				!strncmp(gEngfuncs.hudGetModelByIndex(ent->curstate.modelindex)->name, "sprites/xbeam1.spr", 19)) {
				int beamflags = (ent->curstate.rendermode & 0xF0);
				int noise = ent->curstate.body;
				float scrollrate = ent->curstate.animtime;
				if ((beamflags == 16 && scrollrate == 5.0f && noise == 20) ||
					(beamflags == 0 && scrollrate == 2.5f && noise == 8)) {
					int index = endent & 0xFFF;
					if (index <= 33 && index >= 1) {
						cl_entity_t* end = gEngfuncs.GetEntityByIndex(index);
						if (end && end == gEngfuncs.GetLocalPlayer()) {
							s_egonParticleColor = { ent->curstate.rendercolor.r, ent->curstate.rendercolor.g, ent->curstate.rendercolor.b };
							s_egonMessageNum = ent->curstate.messagenum;
							s_egonShooter = true;
							ent->curstate.rendercolor = { 0,0,0 };
						}
					}
				}
			}
		}
	}
	else if (type == ET_NORMAL) {
		// filter the vanilla egon spark sprites, the particle ball replaces them
		if (ent->curstate.modelindex > 0 &&
			!strncmp(gEngfuncs.hudGetModelByIndex(ent->curstate.modelindex)->name, "sprites/XSpark1.spr", 19) &&
			ent->curstate.entityType == 1 && ent->curstate.scale == 1.0f && ent->curstate.rendermode == 3 && ent->curstate.renderamt == 255 &&
			ent->curstate.rendercolor.r == 255 && ent->curstate.rendercolor.g == 255 && ent->curstate.rendercolor.b == 255 &&
			ent->curstate.renderfx == 14 && ent->curstate.movetype == 8) {
			return true;
		}
	}
	return false;
}

}
