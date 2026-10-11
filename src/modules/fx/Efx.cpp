// Efx module: shared state, lifecycle and registration. The per-effect
// implementations live in Blood/Shell/Gauss/Egon.cpp.
// Ported from ABCEnchance src/core/module/efxenchance.* (GBK -> UTF-8).
// Deviations from the old build (plan 04 §2 note):
// - particleman is gone: the egon ball already used engine temp-entities and
//   keeps doing so;
// - the view-trace dependency on the (AbcUi) playertrace module is replaced by
//   a direct EV_PlayerTrace in Egon.cpp, keeping this plugin self-contained;
// - the old gauss charge state (iGaussChargeSprite / flGaussStartChargeTime +
//   the gauss_spark.spr precache) was write-only in this build and is dropped.

#include "Efx.h"
#include "EfxInternal.h"

#include <metahook.h>

#include <cl_entity.h>

#include <abc/CvarCmd.h>

#include "../../services/ExtraPrecache.h"
#include "../Module.h"

namespace fx {

cvar_t* ShellEfxCvar;
cvar_t* BloodEfxCvar;
cvar_t* BloodSpriteSpeedCvar;
cvar_t* BloodSpriteNumberCvar;
cvar_t* GaussEfxCvar;
cvar_t* EgonEfxCvar;

PrecacheIndexes Precached = {};
color24 EgonParticleColor = {};
int EgonMessageNum = 0;
bool EgonShooter = false;
float EgonNextShoot = 0.0f;

void init() {
	ShellEfxCvar = abc::CREATE_CVAR("abc_shellefx", "1", abc::FCVAR_VALUE, nullptr);
	BloodEfxCvar = abc::CREATE_CVAR("abc_bloodefx", "1", abc::FCVAR_VALUE, nullptr);
	BloodSpriteSpeedCvar = abc::CREATE_CVAR("abc_bloodsprite_speed", "128", abc::FCVAR_VALUE, nullptr);
	BloodSpriteNumberCvar = abc::CREATE_CVAR("abc_bloodsprite_num", "32", abc::FCVAR_VALUE, nullptr);
	GaussEfxCvar = abc::CREATE_CVAR("abc_gaussefx", "1", abc::FCVAR_VALUE, nullptr);
	EgonEfxCvar = abc::CREATE_CVAR("abc_egonefx", "1", abc::FCVAR_VALUE, nullptr);
}

void newMap() {
	extraprecache::clear();
	EgonMessageNum = 0;
	EgonShooter = false;
	EgonNextShoot = 0;
	Precached.GaussBeam = extraprecache::precache("sprites/laserbeam.spr");
	Precached.GaussWaveBeam = extraprecache::precache("abcenchance/spr/gauss_wave.spr");
	Precached.GaussLoophole = extraprecache::precache("abcenchance/mdl/gauss_loophole.mdl");
	Precached.GunSmoke = extraprecache::precache("sprites/Puff1.spr");
	Precached.EgonBeam = extraprecache::precache("abcenchance/spr/egon_ball.spr");
}

void frame() {
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	if (!local || !EgonEfxCvar || EgonEfxCvar->value <= 0 ||
		EgonMessageNum != local->curstate.messagenum)
		EgonShooter = false;
	if (EgonShooter && gEngfuncs.GetClientTime() >= EgonNextShoot)
		shootEgonParticle();
}

namespace {
class EfxModule final : public abcess::IFeatureModule {
public:
	EfxModule() { abcess::moduleRegistry().add(this); }
	const char* name() const override { return "efx"; }
	void onInit() override { init(); }
	void onNewMap() override { newMap(); }
	void onFrame() override { frame(); }
};
const EfxModule RegisterModule;
}

}
