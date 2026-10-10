#pragma once

// Internal shared state of the efx module (Efx/Blood/Shell/Gauss/Egon.cpp).
// Not part of the public module API - include only from src/modules/Efx/.

#include <metahook.h>

#include <const.h>
#include <cvardef.h>

namespace Efx {

// feature cvars (registered by Efx::Init in Efx.cpp)
extern cvar_t* s_pShellEfx;
extern cvar_t* s_pBloodEfx;
extern cvar_t* s_pBloodSpriteSpeed;
extern cvar_t* s_pBloodSpriteNumber;
extern cvar_t* s_pGaussEfx;
extern cvar_t* s_pEgonEfx;

// per-map precache indexes into the extraPrecache service (filled by Efx::Reset)
struct SEfxPrecache {
	int iGaussBeam;
	int iGaussWaveBeam;
	int iGaussLoophole;
	int iGunSmoke;
	int iEgonBeam;
};
extern SEfxPrecache s_Precache;

// egon ball cadence (written by AddEntity/ShootEgonParticle, read by Efx::Frame)
extern color24 s_egonParticleColor;
extern int s_egonMessageNum;
extern bool s_egonShooter;
extern float s_egonNextShoot;

// defined in Egon.cpp, pumped by Efx::Frame
void ShootEgonParticle();

}
