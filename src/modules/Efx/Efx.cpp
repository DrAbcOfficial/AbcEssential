// Efx enhancements: blood sprite rework, shell eject smoke/fade, gauss beam
// rewrite, egon particle ball.
// Ported from ABCEnchance src/core/module/efxenchance.* (GBK -> UTF-8).
// Deviations from the old build (plan 04 §2 note):
// - particleman is gone: the egon ball already used engine temp-entities and
//   keeps doing so;
// - the view-trace dependency on the (AbcUi) playertrace module is replaced by
//   a direct EV_PlayerTrace here, keeping this plugin self-contained.
#include "Efx.h"

#include <algorithm>

#include <metahook.h>

#include <cl_entity.h>
#include <com_model.h>
#include <const.h>
#include <entity_types.h>
#include <event_api.h>
#include <pm_defs.h>
#include <pmtrace.h>
#include <r_efx.h>

#include <abc/CvarCmd.h>
#include <abc/Math.h>

#include "ExtraPrecache.h"
#include "Palette.h"
#include "../../hooks.h"
#include "../Module.h"

#define GAUSS_WAVE_LENGTH 48
#define GAUSS_LASER_P_WIDTH 2
#define GAUSS_LASER_S_WIDTH 4

namespace {

using abc::CMathlib;

cvar_t* s_pShellEfx;
cvar_t* s_pBloodEfx;
cvar_t* s_pBloodSpriteSpeed;
cvar_t* s_pBloodSpriteNumber;
cvar_t* s_pGaussEfx;
cvar_t* s_pEgonEfx;

struct SEfxVariable {
	int iGaussBeam;
	int iGaussWaveBeam;
	int iGaussChargeSprite;
	int iGaussLoophole;
	float flGaussStartChargeTime;

	int iGunSmoke;

	int iEgonBeam;
};
SEfxVariable s_EfxVariable = {};

color24 s_egonParticleColor;
int s_egonMessageNum = 0;
bool s_egonShooter = false;
float s_egonNextShoot = 0.0f;

void CreateGaussLoophole(pmtrace_t* tr) {
	TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(tr->endpos, gEngfuncs.hudGetModelByIndex(s_EfxVariable.iGaussLoophole));
	if (!pTemp)
		return;
	CMathlib::VectorAngles(tr->plane.normal, pTemp->entity.angles);
	pTemp->entity.angles[0] += 90;
	pTemp->entity.angles[2] += 180;
	pTemp->die = gEngfuncs.GetClientTime() + 1.0f;
	pTemp->flags = FTENT_CLIENTCUSTOM | FTENT_FADEOUT;
	pTemp->callback = [](struct tempent_s* ent, float frametime, float currenttime) {
		ent->entity.curstate.scale = ent->die - currenttime;
	};
}

// 高斯主弹道:穿透、反射、能量衰减,与旧版一致
void DoGaussFire(float fparam1, int bparam1) {
	pmtrace_t tr, beam_tr;
	vec3_t vecForward, vecViewAngle;
	vec3_t vecSrc, vecDest, vecDir, viewOfs;
	vec3_t vecNormal, vecReflect, vecRandom, vecRandomSrc, vecLength;
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	int entignore = local->index;
	bool fHasPunched = false;
	bool fFirstBeam = true;
	int nMaxHits = 10;
	float flDamage = fparam1;

	gEngfuncs.GetViewAngles(vecViewAngle);

	CMathlib::AngleVectors(vecViewAngle, vecForward, nullptr, nullptr);
	CMathlib::VectorCopy(vecForward, vecDir);
	CMathlib::VectorNormalize(vecDir);

	CMathlib::VectorCopy(local->curstate.origin, vecSrc);
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(viewOfs);
	CMathlib::VectorAdd(vecSrc, viewOfs, vecSrc);
	CMathlib::VectorMultipiler(vecForward, 8192);
	CMathlib::VectorAdd(vecSrc, vecForward, vecDest);

	while (flDamage > 10 && nMaxHits > 0) {
		// 第一发射线跟踪
		nMaxHits--;
		// 从枪口开始追踪
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecDest, PM_STUDIO_BOX, entignore, &tr);
		// 高斯直线
		if (tr.allsolid != 0)
			break;
		// 无效命中
		if (tr.ent < 0)
			break;
		// 初始光束
		if (fFirstBeam) {
			local->curstate.effects |= EF_MUZZLEFLASH;
			fFirstBeam = false;
			gEngfuncs.pEfxAPI->R_BeamEntPoint(local->index + (gExportfuncs.CL_IsThirdPerson() ? 0 : 4096), tr.endpos,
				s_EfxVariable.iGaussBeam, 0.2f,
				bparam1 ? (float)GAUSS_LASER_P_WIDTH : (float)GAUSS_LASER_S_WIDTH, 0, 1, 0, 0, 0, 1,
				bparam1 ? 0.8f : 1.0f, bparam1 ? 0.0f : 1.0f);
		}
		else
			gEngfuncs.pEfxAPI->R_BeamPoints(vecSrc, tr.endpos, s_EfxVariable.iGaussBeam, 0.2f,
				bparam1 ? (float)GAUSS_LASER_P_WIDTH : (float)GAUSS_LASER_S_WIDTH, 0, 1, 0, 0, 0, 1,
				bparam1 ? 0.8f : 1.0f, bparam1 ? 0.0f : 1.0f);
		cl_entity_t* hit = gEngfuncs.GetEntityByIndex(tr.ent);
		// 命中世界刷子
		if (hit && hit->model && hit->model->type == mod_brush) {
			// 生成弹孔模型
			if (s_EfxVariable.iGaussLoophole)
				CreateGaussLoophole(&tr);
			// 穿透后续实体
			entignore = -1;
			// 平面法线与射线夹角决定反射还是穿透
			CMathlib::VectorCopy(tr.plane.normal, vecNormal);
			float n = -CMathlib::DotProduct(vecNormal, vecDir);
			// 角度小于60度
			if (n < 0.5) {
				// reflect: 反射方向取镜面矢量,能量随入射角衰减
				vecReflect[0] = 2.0f * vecNormal[0] * n + vecDir[0];
				vecReflect[1] = 2.0f * vecNormal[1] * n + vecDir[1];
				vecReflect[2] = 2.0f * vecNormal[2] * n + vecDir[2];
				// 取得新的反射方向作为后续射线路径
				CMathlib::VectorCopy(vecReflect, vecDir);
				CMathlib::VectorCopy(tr.endpos, vecSrc);
				vecDest[0] = vecSrc[0] + vecDir[0] * 8192.0f;
				vecDest[1] = vecSrc[1] + vecDir[1] * 8192.0f;
				vecDest[2] = vecSrc[2] + vecDir[2] * 8192.0f;
				// 随机发散的反射波形光束
				for (int i = 0; i < CMathlib::RANDOM_LONG(0, 4); i++) {
					vecRandom[0] = vecSrc[0] + CMathlib::RANDOM_FLOAT(GAUSS_WAVE_LENGTH / 2, GAUSS_WAVE_LENGTH) *
						(vecNormal[0] * n * CMathlib::RANDOM_FLOAT(1, 3) + vecDir[0] * CMathlib::RANDOM_FLOAT(-3, 3));
					vecRandom[1] = vecSrc[1] + CMathlib::RANDOM_FLOAT(GAUSS_WAVE_LENGTH / 2, GAUSS_WAVE_LENGTH) *
						(vecNormal[1] * n * CMathlib::RANDOM_FLOAT(1, 3) + vecDir[1] * CMathlib::RANDOM_FLOAT(-3, 3));
					vecRandom[2] = vecSrc[2] + CMathlib::RANDOM_FLOAT(GAUSS_WAVE_LENGTH / 2, GAUSS_WAVE_LENGTH) *
						(vecNormal[2] * n * CMathlib::RANDOM_FLOAT(1, 3) + vecDir[2] * CMathlib::RANDOM_FLOAT(-3, 3));
					gEngfuncs.pEfxAPI->R_BeamPoints(vecSrc, vecRandom, s_EfxVariable.iGaussWaveBeam, 0.2f,
						bparam1 ? (float)GAUSS_LASER_P_WIDTH : (float)GAUSS_LASER_S_WIDTH, 0, 1, 0, 0, 0, 1, 0.8f, 0);
				}
				// lose energy
				if (n == 0)
					n = 0.1f;
				flDamage *= 1 - n;
			}
			else {
				// 穿透分支:一拳已经打过就不再穿透
				if (fHasPunched)
					break;
				fHasPunched = true;
				// 右键没有直射光束,尝试从命中点向墙体背面穿透
				if (!bparam1) {
					vecSrc[0] = tr.endpos[0] + vecDir[0] * 8;
					vecSrc[1] = tr.endpos[1] + vecDir[1] * 8;
					vecSrc[2] = tr.endpos[2] + vecDir[2] * 8;
					// 从命中点向前 8 单位再穿一次,以命中背面为终点
					gEngfuncs.pEventAPI->EV_SetTraceHull(2);
					gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecDest, PM_STUDIO_BOX, entignore, &beam_tr);
					// 未触发穿墙光束
					if (beam_tr.allsolid == 0) {
						// 从第一次穿透点为起点,向回穿到入射面,得到墙体厚度
						gEngfuncs.pEventAPI->EV_SetTraceHull(2);
						gEngfuncs.pEventAPI->EV_PlayerTrace(beam_tr.endpos, tr.endpos, PM_STUDIO_BOX, entignore, &beam_tr);
						// 光束长度即 n
						CMathlib::VectorSubtract(beam_tr.endpos, tr.endpos, vecLength);
						n = (float)CMathlib::VectorLength(vecLength);
						// 厚度比伤害小才继续消耗
						CMathlib::VectorCopy(beam_tr.endpos, vecRandomSrc);
						vecRandomSrc[0] += vecDir[0] * flDamage;
						vecRandomSrc[1] += vecDir[1] * flDamage;
						vecRandomSrc[2] += vecDir[2] * flDamage;
						for (int i = 0; i < CMathlib::RANDOM_LONG(8, 14); i++) {
							vecRandom[0] = vecRandomSrc[0] + vecDir[0] * CMathlib::RANDOM_FLOAT(-512, 512);
							vecRandom[1] = vecRandomSrc[1] + vecDir[1] * CMathlib::RANDOM_FLOAT(-512, 512);
							vecRandom[2] = vecRandomSrc[2] + vecDir[2] * CMathlib::RANDOM_FLOAT(-512, 512);
							gEngfuncs.pEventAPI->EV_SetTraceHull(2);
							gEngfuncs.pEventAPI->EV_PlayerTrace(vecRandomSrc, vecRandom, PM_STUDIO_IGNORE, entignore, &beam_tr);
							if (beam_tr.ent == 0)
								CreateGaussLoophole(&beam_tr);
						}
						if (n < flDamage) {
							// n==0 时按 1 处理
							if (n == 0)
								n = 1;
							// 每穿透一层减去厚度
							flDamage -= n;
							// 下一段以穿出点为起点继续
							vecSrc[0] = beam_tr.endpos[0] + vecDir[0];
							vecSrc[1] = beam_tr.endpos[1] + vecDir[1];
							vecSrc[2] = beam_tr.endpos[2] + vecDir[2];
						}
					}
					else
						// 被完全阻挡,停止循环
						flDamage = 0;
				}
				else {
					// 大于60度,主弹道穿透一次后停止
					flDamage = 0;
				}
			}
		}
		else {
			// 命中实体:推进到命中点后继续穿透
			CMathlib::VectorAdd(tr.endpos, vecDir, vecSrc);
			entignore = tr.ent;
		}
	}
}

void EV_StopPreviousGauss() {
	int idx = gEngfuncs.GetViewModel()->index;
	// Make sure we don't have a gauss spin event in the queue for this guy
	gEngfuncs.pEventAPI->EV_KillEvents(idx, "events/gaussspin.sc");
	//SND_STOP 32
	hooks::gHooks.CClient_SoundEngine_PlayFMODSound(hooks::gHooks.pClientSoundEngine, 0, 32, idx, 0,
		CHAN_WEAPON, "ambience/pulsemachine.wav", 0.0f, 0.0f, 0, 0, -1, 0.0f);
}

void ShootEgonParticle() {
	model_t* pModel = gEngfuncs.hudGetModelByIndex(s_EfxVariable.iEgonBeam);
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

}

namespace Efx {

void Init() {
	s_pShellEfx = abc::CREATE_CVAR("abc_shellefx", "1", abc::FCVAR_VALUE, nullptr);
	s_pBloodEfx = abc::CREATE_CVAR("abc_bloodefx", "1", abc::FCVAR_VALUE, nullptr);
	s_pBloodSpriteSpeed = abc::CREATE_CVAR("abc_bloodsprite_speed", "128", abc::FCVAR_VALUE, nullptr);
	s_pBloodSpriteNumber = abc::CREATE_CVAR("abc_bloodsprite_num", "32", abc::FCVAR_VALUE, nullptr);
	s_pGaussEfx = abc::CREATE_CVAR("abc_gaussefx", "1", abc::FCVAR_VALUE, nullptr);
	s_pEgonEfx = abc::CREATE_CVAR("abc_egonefx", "1", abc::FCVAR_VALUE, nullptr);
}

void Reset() {
	ExtraPrecache_Clear();
	s_egonMessageNum = 0;
	s_egonNextShoot = 0;
	s_EfxVariable.iGaussBeam = ExtraPrecache_Precache("sprites/laserbeam.spr");
	s_EfxVariable.iGaussWaveBeam = ExtraPrecache_Precache("abcenchance/spr/gauss_wave.spr");
	s_EfxVariable.iGaussChargeSprite = ExtraPrecache_Precache("abcenchance/spr/gauss_spark.spr");
	s_EfxVariable.iGaussLoophole = ExtraPrecache_Precache("abcenchance/mdl/gauss_loophole.mdl");
	s_EfxVariable.iGunSmoke = ExtraPrecache_Precache("sprites/Puff1.spr");
	s_EfxVariable.iEgonBeam = ExtraPrecache_Precache("abcenchance/spr/egon_ball.spr");
}

void Frame() {
	auto local = gEngfuncs.GetLocalPlayer();
	if (local && s_egonMessageNum != local->curstate.messagenum)
		s_egonShooter = false;
	if (s_egonShooter && gEngfuncs.GetClientTime() >= s_egonNextShoot)
		ShootEgonParticle();
}

void R_BloodSprite(float* org, int colorindex, int modelIndex, int modelIndex2, float size) {
	if (s_pBloodEfx->value > 0) {
		model_t* pModel = gEngfuncs.hudGetModelByIndex(modelIndex);
		if (!pModel)
			return;
		TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(org, pModel);
		if (pTemp) {
			int i;
			vec3_t offset, dir;
			vec3_t forward, right, up;
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
			up[0] = right[0] = forward[0] = 0.0f;
			up[1] = right[1] = forward[1] = 0.0f;
			up[2] = right[2] = forward[2] = 1.0f;
			for (i = 0; i < s_pBloodSpriteNumber->value; i++) {
				CMathlib::VectorCopy(org, offset);
				CMathlib::VectorMA(offset, CMathlib::RANDOM_FLOAT(-0.5f, 0.5f) * size, right, offset);
				CMathlib::VectorMA(offset, CMathlib::RANDOM_FLOAT(-0.5f, 0.5f) * size, up, offset);
				pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(org, gEngfuncs.hudGetModelByIndex(modelIndex2));
				if (!pTemp)
					return;
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
}

TEMPENTITY* R_TempModel(float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype, RTempModelFn original) {
	TEMPENTITY* tent = original(pos, dir, angles, life, modelIndex, soundtype);
	if (tent && s_pShellEfx->value > 0 && soundtype > 0) {
		tent->flags |= FTENT_COLLIDEALL | FTENT_SLOWGRAVITY | FTENT_CLIENTCUSTOM;
		tent->entity.curstate.rendermode = kRenderTransTexture;
		tent->entity.curstate.renderamt = 255;
		tent->fadeSpeed = gEngfuncs.GetClientTime() + life;
		tent->callback = [](struct tempent_s* ent, float frametime, float currenttime) {
			ent->entity.curstate.renderamt = (int)((ent->fadeSpeed - currenttime) * 255.0f);
		};
		vec3_t smokedir = { dir[0], dir[1], CMathlib::RANDOM_FLOAT(70, 155) };
		// smoke goes through the original entry point: no re-enhancement loop
		TEMPENTITY* smoke = original(pos, smokedir, angles, life / 2, s_EfxVariable.iGunSmoke, 0);
		if (smoke) {
			smoke->flags = FTENT_SPRANIMATE | FTENT_FADEOUT;
			smoke->entity.baseline.scale = CMathlib::RANDOM_FLOAT(0.07f, 0.15f);
			smoke->entity.curstate.renderamt = CMathlib::RANDOM_LONG(10, 25);
			smoke->entity.curstate.rendermode = kRenderTransAlpha;
			smoke->entity.curstate.framerate = CMathlib::RANDOM_FLOAT(25, 35);
			smoke->fadeSpeed = 20.0f;
		}
	}
	return tent;
}

bool pfnPlaybackEvent(int flags, const struct edict_s* pInvoker, unsigned short eventindex, float delay,
	float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2) {
	// 高斯主开火 flag 3, index 12;高斯转轮 flag1, index 13
	if (eventindex == 12 && s_pGaussEfx->value > 0) {
		EV_StopPreviousGauss();
		gEngfuncs.pEventAPI->EV_WeaponAnimation(6, 0);
		auto view = gEngfuncs.GetViewModel();
		hooks::gHooks.CClient_SoundEngine_PlayFMODSound(hooks::gHooks.pClientSoundEngine, 0, 0, view->index, view->origin,
			CHAN_WEAPON, "weapons/gauss2.wav", 1.0f, 1.0f, 0, 100, -1, 0.0f);
		hooks::gHooks.V_PunchAxis(0, -1.5f);
		// f1 伤害;b1 是否为副开火
		DoGaussFire(fparam1, bparam1);
		return true;
	}
	return false;
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
