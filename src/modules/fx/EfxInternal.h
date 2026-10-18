#pragma once

// Internal shared state of the efx module (Efx/Blood/Shell/Gauss/Egon.cpp).
// Not part of the public module API - include only from src/modules/fx/.

#include <metahook.h>

#include <const.h>
#include <cvardef.h>

namespace fx {

// feature cvars (registered by fx::init in Efx.cpp)
extern cvar_t *ShellEfxCvar;
extern cvar_t *BloodEfxCvar;
extern cvar_t *BloodSpriteSpeedCvar;
extern cvar_t *BloodSpriteNumberCvar;
extern cvar_t *GaussEfxCvar;
extern cvar_t *EgonEfxCvar;

// per-map precache indexes into the extraprecache service (filled by newMap)
struct PrecacheIndexes {
  int GaussBeam;
  int GaussWaveBeam;
  int GaussLoophole;
  int GunSmoke;
  int EgonBeam;
};
extern PrecacheIndexes Precached;

// egon ball cadence (written by addEntity/shootEgonParticle, read by frame())
extern color24 EgonParticleColor;
extern int EgonMessageNum;
extern bool EgonShooter;
extern float EgonNextShoot;

// defined in Egon.cpp, pumped by frame()
void shootEgonParticle();

} // namespace fx
