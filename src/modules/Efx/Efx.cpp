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

namespace Efx {

cvar_t* s_pShellEfx;
cvar_t* s_pBloodEfx;
cvar_t* s_pBloodSpriteSpeed;
cvar_t* s_pBloodSpriteNumber;
cvar_t* s_pGaussEfx;
cvar_t* s_pEgonEfx;

SEfxPrecache s_Precache = {};
color24 s_egonParticleColor = {};
int s_egonMessageNum = 0;
bool s_egonShooter = false;
float s_egonNextShoot = 0.0f;

void Init() {
	s_pShellEfx = abc::CREATE_CVAR("abc_shellefx", "1", abc::FCVAR_VALUE, nullptr);
	s_pBloodEfx = abc::CREATE_CVAR("abc_bloodefx", "1", abc::FCVAR_VALUE, nullptr);
	s_pBloodSpriteSpeed = abc::CREATE_CVAR("abc_bloodsprite_speed", "128", abc::FCVAR_VALUE, nullptr);
	s_pBloodSpriteNumber = abc::CREATE_CVAR("abc_bloodsprite_num", "32", abc::FCVAR_VALUE, nullptr);
	s_pGaussEfx = abc::CREATE_CVAR("abc_gaussefx", "1", abc::FCVAR_VALUE, nullptr);
	s_pEgonEfx = abc::CREATE_CVAR("abc_egonefx", "1", abc::FCVAR_VALUE, nullptr);
}

void Reset() {
	extraPrecache::clear();
	s_egonMessageNum = 0;
	s_egonNextShoot = 0;
	s_Precache.iGaussBeam = extraPrecache::precache("sprites/laserbeam.spr");
	s_Precache.iGaussWaveBeam = extraPrecache::precache("abcenchance/spr/gauss_wave.spr");
	s_Precache.iGaussLoophole = extraPrecache::precache("abcenchance/mdl/gauss_loophole.mdl");
	s_Precache.iGunSmoke = extraPrecache::precache("sprites/Puff1.spr");
	s_Precache.iEgonBeam = extraPrecache::precache("abcenchance/spr/egon_ball.spr");
}

void Frame() {
	auto local = gEngfuncs.GetLocalPlayer();
	if (local && s_egonMessageNum != local->curstate.messagenum)
		s_egonShooter = false;
	if (s_egonShooter && gEngfuncs.GetClientTime() >= s_egonNextShoot)
		ShootEgonParticle();
}

}

namespace {
class EfxModule final : public abcess::IFeatureModule {
public:
	EfxModule() { abcess::moduleRegistry().add(this); }
	const char* name() const override { return "efx"; }
	void onInit() override { Efx::Init(); }
	void onNewMap() override { Efx::Reset(); }
	void onFrame() override { Efx::Frame(); }
};
const EfxModule RegisterModule;
}
