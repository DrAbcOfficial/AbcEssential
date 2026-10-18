// Gauss primary-fire rework: piercing/reflection beam rewrite + loopholes.
// Ported from ABCEnchance src/core/module/efxenchance.cpp DoGaussFire part.
// Uses only direct-call scanned client functions from hooks::Hooks
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

namespace fx {

namespace {

using abc::CMathlib;

// beam tuning (old GAUSS_* macros)
constexpr float kGaussWaveLength = 48.0f;
constexpr float kGaussLaserPrimaryWidth = 2.0f;
constexpr float kGaussLaserSecondaryWidth = 4.0f;

void createGaussLoophole(pmtrace_t *trace) {
  model_t *Model = Precached.GaussLoophole >= 0
                       ? gEngfuncs.hudGetModelByIndex(Precached.GaussLoophole)
                       : nullptr;
  if (!Model)
    return;
  TEMPENTITY *temp =
      gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(trace->endpos, Model);
  if (!temp)
    return;
  CMathlib::VectorAngles(trace->plane.normal, temp->entity.angles);
  temp->entity.angles[0] += 90;
  temp->entity.angles[2] += 180;
  temp->die = gEngfuncs.GetClientTime() + 1.0f;
  temp->flags = FTENT_CLIENTCUSTOM | FTENT_FADEOUT;
  temp->callback = [](struct tempent_s *ent, float frametime,
                      float currenttime) {
    ent->entity.curstate.scale = ent->die - currenttime;
  };
}

void stopPreviousGaussSound() {
  int viewModelIndex = gEngfuncs.GetViewModel()->index;
  // Make sure we don't have a gauss spin event in the queue for this guy
  gEngfuncs.pEventAPI->EV_KillEvents(viewModelIndex, "events/gaussspin.sc");
  // SND_STOP 32
  hooks::Hooks.CClient_SoundEngine_PlayFMODSound(
      hooks::Hooks.ClientSoundEngine, 0, 32, viewModelIndex, 0, CHAN_WEAPON,
      "ambience/pulsemachine.wav", 0.0f, 0.0f, 0, 0, -1, 0.0f);
}

} // namespace

// 高斯主弹道:穿透、反射、能量衰减,与旧版一致
static void gaussFire(float damage, bool secondary) {
  pmtrace_t trace, beamTrace;
  vec3_t forward, viewAngles;
  vec3_t src, dest, dir, viewOfs;
  vec3_t normal, reflect, random, randomSrc, segment;
  cl_entity_t *local = gEngfuncs.GetLocalPlayer();
  int entIgnore = local->index;
  bool hasPunched = false;
  bool firstBeam = true;
  int maxHits = 10;

  gEngfuncs.GetViewAngles(viewAngles);

  CMathlib::AngleVectors(viewAngles, forward, nullptr, nullptr);
  CMathlib::VectorCopy(forward, dir);
  CMathlib::VectorNormalize(dir);

  CMathlib::VectorCopy(local->curstate.origin, src);
  gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(viewOfs);
  CMathlib::VectorAdd(src, viewOfs, src);
  CMathlib::VectorMultipiler(forward, 8192);
  CMathlib::VectorAdd(src, forward, dest);

  while (damage > 10 && maxHits > 0) {
    // 第一发射线跟踪
    maxHits--;
    // 从枪口开始追踪
    gEngfuncs.pEventAPI->EV_SetTraceHull(2);
    gEngfuncs.pEventAPI->EV_PlayerTrace(src, dest, PM_STUDIO_BOX, entIgnore,
                                        &trace);
    // 高斯直线
    if (trace.allsolid != 0)
      break;
    // 无效命中
    if (trace.ent < 0)
      break;
    // 初始光束
    if (firstBeam) {
      local->curstate.effects |= EF_MUZZLEFLASH;
      firstBeam = false;
      gEngfuncs.pEfxAPI->R_BeamEntPoint(
          local->index + (gExportfuncs.CL_IsThirdPerson() ? 0 : 4096),
          trace.endpos, Precached.GaussBeam, 0.2f,
          secondary ? kGaussLaserPrimaryWidth : kGaussLaserSecondaryWidth, 0, 1,
          0, 0, 0, 1, secondary ? 0.8f : 1.0f, secondary ? 0.0f : 1.0f);
    } else
      gEngfuncs.pEfxAPI->R_BeamPoints(
          src, trace.endpos, Precached.GaussBeam, 0.2f,
          secondary ? kGaussLaserPrimaryWidth : kGaussLaserSecondaryWidth, 0, 1,
          0, 0, 0, 1, secondary ? 0.8f : 1.0f, secondary ? 0.0f : 1.0f);
    cl_entity_t *hit = gEngfuncs.GetEntityByIndex(
        gEngfuncs.pEventAPI->EV_IndexFromTrace(&trace));
    // 命中世界刷子
    if (hit && hit->model && hit->model->type == mod_brush) {
      // 生成弹孔模型
      if (Precached.GaussLoophole >= 0)
        createGaussLoophole(&trace);
      // 穿透后续实体
      entIgnore = -1;
      // 平面法线与射线夹角决定反射还是穿透
      CMathlib::VectorCopy(trace.plane.normal, normal);
      float incidence = -CMathlib::DotProduct(normal, dir);
      // 角度小于60度
      if (incidence < 0.5) {
        // reflect: 反射方向取镜面矢量,能量随入射角衰减
        reflect[0] = 2.0f * normal[0] * incidence + dir[0];
        reflect[1] = 2.0f * normal[1] * incidence + dir[1];
        reflect[2] = 2.0f * normal[2] * incidence + dir[2];
        // 取得新的反射方向作为后续射线路径
        CMathlib::VectorCopy(reflect, dir);
        CMathlib::VectorCopy(trace.endpos, src);
        dest[0] = src[0] + dir[0] * 8192.0f;
        dest[1] = src[1] + dir[1] * 8192.0f;
        dest[2] = src[2] + dir[2] * 8192.0f;
        // 随机发散的反射波形光束
        const int Beams = CMathlib::RANDOM_LONG(0, 4);
        for (int i = 0; i < Beams; i++) {
          random[0] =
              src[0] +
              CMathlib::RANDOM_FLOAT(kGaussWaveLength / 2, kGaussWaveLength) *
                  (normal[0] * incidence * CMathlib::RANDOM_FLOAT(1, 3) +
                   dir[0] * CMathlib::RANDOM_FLOAT(-3, 3));
          random[1] =
              src[1] +
              CMathlib::RANDOM_FLOAT(kGaussWaveLength / 2, kGaussWaveLength) *
                  (normal[1] * incidence * CMathlib::RANDOM_FLOAT(1, 3) +
                   dir[1] * CMathlib::RANDOM_FLOAT(-3, 3));
          random[2] =
              src[2] +
              CMathlib::RANDOM_FLOAT(kGaussWaveLength / 2, kGaussWaveLength) *
                  (normal[2] * incidence * CMathlib::RANDOM_FLOAT(1, 3) +
                   dir[2] * CMathlib::RANDOM_FLOAT(-3, 3));
          gEngfuncs.pEfxAPI->R_BeamPoints(
              src, random, Precached.GaussWaveBeam, 0.2f,
              secondary ? kGaussLaserPrimaryWidth : kGaussLaserSecondaryWidth,
              0, 1, 0, 0, 0, 1, 0.8f, 0);
        }
        // lose energy
        if (incidence == 0)
          incidence = 0.1f;
        damage *= 1 - incidence;
      } else {
        // 穿透分支:一拳已经打过就不再穿透
        if (hasPunched)
          break;
        hasPunched = true;
        // 右键没有直射光束,尝试从命中点向墙体背面穿透
        if (!secondary) {
          src[0] = trace.endpos[0] + dir[0] * 8;
          src[1] = trace.endpos[1] + dir[1] * 8;
          src[2] = trace.endpos[2] + dir[2] * 8;
          // 从命中点向前 8 单位再穿一次,以命中背面为终点
          gEngfuncs.pEventAPI->EV_SetTraceHull(2);
          gEngfuncs.pEventAPI->EV_PlayerTrace(src, dest, PM_STUDIO_BOX,
                                              entIgnore, &beamTrace);
          // 未触发穿墙光束
          if (beamTrace.allsolid == 0) {
            // 从第一次穿透点为起点,向回穿到入射面,得到墙体厚度
            gEngfuncs.pEventAPI->EV_SetTraceHull(2);
            gEngfuncs.pEventAPI->EV_PlayerTrace(beamTrace.endpos, trace.endpos,
                                                PM_STUDIO_BOX, entIgnore,
                                                &beamTrace);
            // 光束长度即厚度
            CMathlib::VectorSubtract(beamTrace.endpos, trace.endpos, segment);
            float thickness = (float)CMathlib::VectorLength(segment);
            // 厚度比伤害小才继续消耗
            CMathlib::VectorCopy(beamTrace.endpos, randomSrc);
            randomSrc[0] += dir[0] * damage;
            randomSrc[1] += dir[1] * damage;
            randomSrc[2] += dir[2] * damage;
            const int Decorations = CMathlib::RANDOM_LONG(8, 14);
            for (int i = 0; i < Decorations; i++) {
              random[0] =
                  randomSrc[0] + dir[0] * CMathlib::RANDOM_FLOAT(-512, 512);
              random[1] =
                  randomSrc[1] + dir[1] * CMathlib::RANDOM_FLOAT(-512, 512);
              random[2] =
                  randomSrc[2] + dir[2] * CMathlib::RANDOM_FLOAT(-512, 512);
              gEngfuncs.pEventAPI->EV_SetTraceHull(2);
              pmtrace_t DecorationTrace{};
              gEngfuncs.pEventAPI->EV_PlayerTrace(randomSrc, random,
                                                  PM_STUDIO_IGNORE, entIgnore,
                                                  &DecorationTrace);
              if (DecorationTrace.ent == 0)
                createGaussLoophole(&DecorationTrace);
            }
            if (thickness < damage) {
              // 厚度==0 时按 1 处理
              if (thickness == 0)
                thickness = 1;
              // 每穿透一层减去厚度
              damage -= thickness;
              // 下一段以穿出点为起点继续
              src[0] = beamTrace.endpos[0] + dir[0];
              src[1] = beamTrace.endpos[1] + dir[1];
              src[2] = beamTrace.endpos[2] + dir[2];
            }
          } else
            // 被完全阻挡,停止循环
            damage = 0;
        } else {
          // 大于60度,主弹道穿透一次后停止
          damage = 0;
        }
      }
    } else {
      // 命中实体:推进到命中点后继续穿透
      CMathlib::VectorAdd(trace.endpos, dir, src);
      entIgnore = trace.ent;
    }
  }
}

bool playbackEvent(int flags, const struct edict_s *pInvoker,
                   unsigned short eventindex, float delay, float *origin,
                   float *angles, float fparam1, float fparam2, int iparam1,
                   int iparam2, int bparam1, int bparam2) {
  // 高斯主开火 flag 3, index 12;高斯转轮 flag1, index 13
  if (eventindex == 12 && GaussEfxCvar && GaussEfxCvar->value > 0 &&
      Precached.GaussBeam >= 0 && Precached.GaussWaveBeam >= 0 &&
      gEngfuncs.GetLocalPlayer() && gEngfuncs.GetViewModel()) {
    stopPreviousGaussSound();
    gEngfuncs.pEventAPI->EV_WeaponAnimation(6, 0);
    cl_entity_t *view = gEngfuncs.GetViewModel();
    hooks::Hooks.CClient_SoundEngine_PlayFMODSound(
        hooks::Hooks.ClientSoundEngine, 0, 0, view->index, view->origin,
        CHAN_WEAPON, "weapons/gauss2.wav", 1.0f, 1.0f, 0, 100, -1, 0.0f);
    hooks::Hooks.V_PunchAxis(0, -1.5f);
    // f1 伤害;b1 是否为副开火
    gaussFire(fparam1, bparam1 != 0);
    return true;
  }
  return false;
}

} // namespace fx
