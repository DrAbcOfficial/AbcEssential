#pragma once

// Prediction weapon domain.
//
// svencoop_sdk/common/entity_state.h: local_state_t::weapondata[256];
// cl_dll/hl/hl_weapons.h: MAX_WEAPONS 256. The HUD prediction transfer
// indexes the array by the full server weapon id, so the signal layer must
// keep the same domain instead of an artificial 64-slot cap.
namespace signals {

inline constexpr int kPredictionWeaponCount = 256;

inline bool validWeaponId(int id) {
  return id > 0 && id < kPredictionWeaponCount;
}

} // namespace signals
