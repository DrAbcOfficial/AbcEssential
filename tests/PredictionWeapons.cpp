// Offline unit test for the prediction weapon domain.
//
// svencoop_sdk/common/entity_state.h gives the engine 256 weapon-data slots;
// the signal layer must accept ids 64..255 (the old 64-slot cap silently
// dropped their add_weapon/drop_weapon events) and reject ids that would
// index past the array.
#include "modules/autofunc/PredictionWeapons.h"

#include <cstdio>

namespace {

int Failures = 0;

void check(bool condition, const char *what) {
  if (condition)
    return;
  std::printf("FAIL: %s\n", what);
  ++Failures;
}

} // namespace

int main() {
  check(signals::kPredictionWeaponCount == 256,
        "the prediction array has 256 entries");
  check(signals::validWeaponId(1), "id 1");
  check(signals::validWeaponId(63), "id 63");
  check(signals::validWeaponId(64), "id 64 (past the old cap)");
  check(signals::validWeaponId(255), "id 255");
  check(!signals::validWeaponId(0), "id 0");
  check(!signals::validWeaponId(-1), "negative id");
  check(!signals::validWeaponId(256), "id 256");
  check(!signals::validWeaponId(30000), "far out-of-range id");

  if (Failures == 0)
    std::printf("PASS: abc_essential_prediction_weapons\n");
  return Failures == 0 ? 0 : 1;
}
