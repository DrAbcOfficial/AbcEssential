// Blood sprite rework: palette-tinted spray + ground droplets.
// Ported from ABCEnchance src/core/module/efxenchance.cpp R_BloodSprite.
// Pure enhancement pass - the spine chains the engine original (R-2).

#include "EfxInternal.h"
#include "Palette.h"

#include <algorithm>

#include <metahook.h>

#include <com_model.h>
#include <r_efx.h>

#include <abc/Math.h>

namespace Efx {

using abc::CMathlib;

void R_BloodSprite(float* org, int colorindex, int modelIndex, int modelIndex2, float size) {
	if (s_pBloodEfx->value <= 0)
		return;
	model_t* pModel = gEngfuncs.hudGetModelByIndex(modelIndex);
	if (!pModel)
		return;
	TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(org, pModel);
	if (!pTemp)
		return;
	int nColor = std::clamp<int>(colorindex, 0, 255);
	const color24& palette = ABC_SVENCOOP_PALETTE[nColor];
	pTemp->entity.curstate.scale = CMathlib::RANDOM_FLOAT((size / 25.0f), (size / 35.0f));
	pTemp->flags = FTENT_SPRANIMATE;
	pTemp->entity.curstate.rendercolor.r = palette.r;
	pTemp->entity.curstate.rendercolor.g = palette.g;
	pTemp->entity.curstate.rendercolor.b = palette.b;
	pTemp->entity.curstate.framerate = 10.0f * pModel->numframes; //1s
	pTemp->die = gEngfuncs.GetClientTime() + (pModel->numframes / pTemp->entity.curstate.framerate);
	pTemp->entity.angles[2] = CMathlib::RANDOM_FLOAT(0, 360);
	pTemp->bounceFactor = 0;
	vec3_t offset, dir;
	vec3_t forward, right, up;
	up[0] = right[0] = forward[0] = 0.0f;
	up[1] = right[1] = forward[1] = 0.0f;
	up[2] = right[2] = forward[2] = 1.0f;
	for (int i = 0; i < s_pBloodSpriteNumber->value; i++) {
		CMathlib::VectorCopy(org, offset);
		CMathlib::VectorMA(offset, CMathlib::RANDOM_FLOAT(-0.5f, 0.5f) * size, right, offset);
		CMathlib::VectorMA(offset, CMathlib::RANDOM_FLOAT(-0.5f, 0.5f) * size, up, offset);
		pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(org, gEngfuncs.hudGetModelByIndex(modelIndex2));
		if (!pTemp)
			break;
		pTemp->flags = FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY;
		pTemp->entity.curstate.scale = CMathlib::RANDOM_FLOAT((size / 25.0f), (size / 35.0f));
		pTemp->entity.curstate.rendercolor.r = palette.r;
		pTemp->entity.curstate.rendercolor.g = palette.g;
		pTemp->entity.curstate.rendercolor.b = palette.b;
		pTemp->die = gEngfuncs.GetClientTime() + CMathlib::RANDOM_FLOAT(1.0f, 3.0f);
		pTemp->entity.angles[2] = gEngfuncs.pfnRandomFloat(0.0f, 360.0f);
		pTemp->bounceFactor = 0;
		srand((unsigned int)gEngfuncs.GetClientTime() * i - i);
		dir[0] = forward[0] + float(rand()) / float(RAND_MAX) * 2.0f - 1;
		srand((unsigned int)gEngfuncs.GetClientTime() * i + i);
		dir[1] = forward[1] + float(rand()) / float(RAND_MAX) * 2.0f - 1;
		dir[2] = forward[2];
		CMathlib::VectorScale(dir, CMathlib::RANDOM_FLOAT(8.0f * size, 20.0f * size), pTemp->entity.baseline.origin);
		pTemp->entity.baseline.origin[0] += CMathlib::RANDOM_FLOAT(4.0f, s_pBloodSpriteSpeed->value) * (size);
		pTemp->entity.baseline.origin[1] += CMathlib::RANDOM_FLOAT(4.0f, s_pBloodSpriteSpeed->value) * (size / 2);
		pTemp->entity.baseline.origin[2] += CMathlib::RANDOM_FLOAT(4.0f, s_pBloodSpriteSpeed->value) * (size / 4);
	}
}

}
