// Blood sprite rework: palette-tinted spray + ground droplets.
// Ported from ABCEnchance src/core/module/efxenchance.cpp R_BloodSprite.
// Pure enhancement pass - the spine chains the engine original (R-2).

#include "EfxInternal.h"
#include "Palette.h"

#include <algorithm>
#include <cmath>

#include <metahook.h>

#include <com_model.h>
#include <r_efx.h>

#include <abc/Math.h>

namespace fx {

using abc::CMathlib;

void bloodSprite(float* org, int colorindex, int modelIndex, int modelIndex2, float size) {
	if (BloodEfxCvar->value <= 0 || !std::isfinite(size) || size <= 0)
		return;
	size = (std::min)(size, 64.0f);
	model_t* spriteModel = gEngfuncs.hudGetModelByIndex(modelIndex);
	if (!spriteModel || spriteModel->numframes <= 0)
		return;
	TEMPENTITY* sprite = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(org, spriteModel);
	if (!sprite)
		return;
	int paletteIndex = std::clamp<int>(colorindex, 0, 255);
	const color24& paletteColor = kSvenCoopPalette[paletteIndex];
	sprite->entity.curstate.scale = CMathlib::RANDOM_FLOAT((size / 25.0f), (size / 35.0f));
	sprite->flags = FTENT_SPRANIMATE;
	sprite->entity.curstate.rendercolor.r = paletteColor.r;
	sprite->entity.curstate.rendercolor.g = paletteColor.g;
	sprite->entity.curstate.rendercolor.b = paletteColor.b;
	sprite->entity.curstate.framerate = 10.0f * spriteModel->numframes; //1s
	sprite->die = gEngfuncs.GetClientTime() + (spriteModel->numframes / sprite->entity.curstate.framerate);
	sprite->entity.angles[2] = CMathlib::RANDOM_FLOAT(0, 360);
	sprite->bounceFactor = 0;
	vec3_t offset, dir;
	const vec3_t right = {1.0f, 0.0f, 0.0f};
	const vec3_t up = {0.0f, 1.0f, 0.0f};
	model_t* DropletModel = gEngfuncs.hudGetModelByIndex(modelIndex2);
	if (!DropletModel)
		return;
	const float Requested = BloodSpriteNumberCvar->value;
	const int Count = std::isfinite(Requested) ? static_cast<int>(std::clamp(Requested, 0.0f, 128.0f)) : 0;
	const float Speed = BloodSpriteSpeedCvar->value;
	const float MaxSpeed = std::isfinite(Speed) ? std::clamp(Speed, 4.0f, 1024.0f) : 4.0f;
	for (int i = 0; i < Count; i++) {
		CMathlib::VectorCopy(org, offset);
		CMathlib::VectorMA(offset, CMathlib::RANDOM_FLOAT(-0.5f, 0.5f) * size, right, offset);
		CMathlib::VectorMA(offset, CMathlib::RANDOM_FLOAT(-0.5f, 0.5f) * size, up, offset);
		sprite = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(offset, DropletModel);
		if (!sprite)
			break;
		sprite->flags = FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY;
		sprite->entity.curstate.scale = CMathlib::RANDOM_FLOAT((size / 25.0f), (size / 35.0f));
		sprite->entity.curstate.rendercolor.r = paletteColor.r;
		sprite->entity.curstate.rendercolor.g = paletteColor.g;
		sprite->entity.curstate.rendercolor.b = paletteColor.b;
		sprite->die = gEngfuncs.GetClientTime() + CMathlib::RANDOM_FLOAT(1.0f, 3.0f);
		sprite->entity.angles[2] = gEngfuncs.pfnRandomFloat(0.0f, 360.0f);
		sprite->bounceFactor = 0;
		dir[0] = gEngfuncs.pfnRandomFloat(-1.0f, 1.0f);
		dir[1] = gEngfuncs.pfnRandomFloat(-1.0f, 1.0f);
		dir[2] = 1.0f;
		CMathlib::VectorScale(dir, CMathlib::RANDOM_FLOAT(8.0f * size, 20.0f * size), sprite->entity.baseline.origin);
		sprite->entity.baseline.origin[0] += CMathlib::RANDOM_FLOAT(4.0f, MaxSpeed) * (size);
		sprite->entity.baseline.origin[1] += CMathlib::RANDOM_FLOAT(4.0f, MaxSpeed) * (size / 2);
		sprite->entity.baseline.origin[2] += CMathlib::RANDOM_FLOAT(4.0f, MaxSpeed) * (size / 4);
	}
}

}
