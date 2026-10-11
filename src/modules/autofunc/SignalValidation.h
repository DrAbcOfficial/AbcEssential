#pragma once

// Event command validation shared by the message boundary and offline tests.
// Persistent names remain defined in Signals.h; no engine state is accessed.
#include "Signals.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace signals {
inline constexpr std::array<std::string_view, 7> kEventNames = {
	kEventDeath, kEventDamage, kEventHealth, kEventBattery,
	kEventFlashBattery, kEventAddWeapon, kEventDropWeapon
};

inline bool isEventName(std::string_view Name) {
	return std::find(kEventNames.begin(), kEventNames.end(), Name) != kEventNames.end();
}

inline bool isWeaponIdentifier(std::string_view Name) {
	return !Name.empty() && Name.size() <= 127 &&
		std::all_of(Name.begin(), Name.end(), [](unsigned char Ch) {
			return (Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') ||
				(Ch >= '0' && Ch <= '9') || Ch == '_';
		});
}
}
