#pragma once

// Event command validation shared by the message boundary and offline tests.
// Persistent names remain defined in Signals.h; no engine state is accessed.
#include "Signals.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace signals {
inline constexpr std::array<std::string_view, 7> kEventNames = {
	kEventDeath, kEventDamage, kEventHealth, kEventBattery,
	kEventFlashBattery, kEventAddWeapon, kEventDropWeapon
};

// Strict 0/1 parse for the `events` operation argument. std::from_chars
// rejects signs, whitespace and trailing garbage; a malformed value must not
// silently become 0 (which would delete a binding).
inline std::optional<int> parseOperation(std::string_view Text) {
	int Value = 0;
	const auto Result = std::from_chars(Text.data(), Text.data() + Text.size(), Value);
	if (Result.ec != std::errc{} || Result.ptr != Text.data() + Text.size()
		|| (Value != 0 && Value != 1))
		return std::nullopt;
	return Value;
}

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
