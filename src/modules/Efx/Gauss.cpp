// Gauss primary-fire rework: piercing/reflection beam rewrite + loopholes.
// Ported from ABCEnchance src/core/module/efxenchance.cpp DoGaussFire part.
// Uses only direct-call scanned client functions from hooks::gHooks
// (CClient_SoundEngine_PlayFMODSound / V_PunchAxis carry no detour); the
// playbackEvent chain itself stays in the spine (R-2).

#include "EfxInternal.h"

#include <metahook.h>

#include <cl_entity.h>
#include <com_model.h>
#include <const.h>
#include <event_api.h>
#include <pm_defs.h>
#include <pmtrace.h>
#include <r_efx.h>

#include <abc/Math.h>

#include "../../hooks.h"

#define GAUSS_WAVE_LENGTH 48
#define GAUSS_LASER_P_WIDTH 2
#define GAUSS_LASER_S_WIDTH 4

namespace Efx {

namespace {

using abc::CMathlib;

void CreateGaussLoophole(pmtrace_t* tr) {
	TEMPENTITY* pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(tr->endpos, gEngfuncs.hudGetModelByIndex(s_Precache.iGaussLoophole));
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

void EV_StopPreviousGauss() {
	int idx = gEngfuncs.GetViewModel()->index;
	// Make sure we don't have a gauss spin event in the queue for this guy
	gEngfuncs.pEventAPI->EV_KillEvents(idx, "events/gaussspin.sc");
	//SND_STOP 32
	hooks::gHooks.CClient_SoundEngine_PlayFMODSound(hooks::gHooks.pClientSoundEngine, 0, 32, idx, 0,
		CHAN_WEAPON, "ambience/pulsemachine.wav", 0.0f, 0.0f, 0, 0, -1, 0.0f);
}

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
				s_Precache.iGaussBeam, 0.2f,
				bparam1 ? (float)GAUSS_LASER_P_WIDTH : (float)GAUSS_LASER_S_WIDTH, 0, 1, 0, 0, 0, 1,
				bparam1 ? 0.8f : 1.0f, bparam1 ? 0.0f : 1.0f);
		}
		else
			gEngfuncs.pEfxAPI->R_BeamPoints(vecSrc, tr.endpos, s_Precache.iGaussBeam, 0.2f,
				bparam1 ? (float)GAUSS_LASER_P_WIDTH : (float)GAUSS_LASER_S_WIDTH, 0, 1, 0, 0, 0, 1,
				bparam1 ? 0.8f : 1.0f, bparam1 ? 0.0f : 1.0f);
		cl_entity_t* hit = gEngfuncs.GetEntityByIndex(tr.ent);
		// 命中世界刷子
		if (hit && hit->model && hit->model->type == mod_brush) {
			// 生成弹孔模型
			if (s_Precache.iGaussLoophole)
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
					gEngfuncs.pEfxAPI->R_BeamPoints(vecSrc, vecRandom, s_Precache.iGaussWaveBeam, 0.2f,
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

}
