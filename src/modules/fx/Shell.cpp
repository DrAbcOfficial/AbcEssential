// Shell-eject rework: fade-out + gun smoke on every ejected shell.
// Ported from ABCEnchance src/core/module/efxenchance.cpp R_TempModel part.
// The original entry is injected by the spine - the smoke shell must spawn
// through the engine API, and modules never touch the hook chain (R-2).

#include "Efx.h"
#include "EfxInternal.h"

#include <metahook.h>

#include <r_efx.h>

#include <abc/Math.h>

namespace fx {

using abc::CMathlib;

TEMPENTITY* tempModel(float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype, TempModelFn original) {
	TEMPENTITY* shell = original(pos, dir, angles, life, modelIndex, soundtype);
	if (shell && ShellEfxCvar && ShellEfxCvar->value > 0 && soundtype > 0) {
		shell->flags |= FTENT_COLLIDEALL | FTENT_SLOWGRAVITY | FTENT_CLIENTCUSTOM;
		shell->entity.curstate.rendermode = kRenderTransTexture;
		shell->entity.curstate.renderamt = 255;
		shell->fadeSpeed = gEngfuncs.GetClientTime() + life;
		shell->callback = [](struct tempent_s* ent, float frametime, float currenttime) {
			ent->entity.curstate.renderamt = (int)((ent->fadeSpeed - currenttime) * 255.0f);
		};
		vec3_t smokeDir = { dir[0], dir[1], CMathlib::RANDOM_FLOAT(70, 155) };
		// smoke goes through the original entry point: no re-enhancement loop
		TEMPENTITY* smoke = original(pos, smokeDir, angles, life / 2, Precached.GunSmoke, 0);
		if (smoke) {
			smoke->flags = FTENT_SPRANIMATE | FTENT_FADEOUT;
			smoke->entity.baseline.scale = CMathlib::RANDOM_FLOAT(0.07f, 0.15f);
			smoke->entity.curstate.renderamt = CMathlib::RANDOM_LONG(10, 25);
			smoke->entity.curstate.rendermode = kRenderTransAlpha;
			smoke->entity.curstate.framerate = CMathlib::RANDOM_FLOAT(25, 35);
			smoke->fadeSpeed = 20.0f;
		}
	}
	return shell;
}

}
