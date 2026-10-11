#pragma once

// Concurrent command graph validation; quoted arguments are not commands.
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace autofunc {
inline std::string canonicalCommand(std::string_view Name) {
	std::string Result(Name);
	for (char& Ch : Result)
		if (Ch >= 'A' && Ch <= 'Z') Ch += 'a' - 'A';
	return Result;
}

inline std::vector<std::string> commandHeads(std::string_view Script) {
	std::vector<std::string> Result;
	bool Quoted = false, AtStart = true;
	for (size_t i = 0; i < Script.size();) {
		const char Ch = Script[i];
		if (!Quoted && (Ch == ';' || Ch == '\n' || Ch == '\r')) { AtStart = true; ++i; continue; }
		if (AtStart && (Ch == ' ' || Ch == '\t')) { ++i; continue; }
		if (AtStart) {
			const size_t Begin = i;
			while (i < Script.size() && Script[i] != ' ' && Script[i] != '\t' && Script[i] != ';' && Script[i] != '\n' && Script[i] != '\r') ++i;
			Result.push_back(canonicalCommand(Script.substr(Begin, i - Begin)));
			AtStart = false;
			continue;
		}
		if (Ch == '"') Quoted = !Quoted;
		++i;
	}
	return Result;
}

inline bool hasCommandCycle(const std::map<std::string, std::string>& Bindings, const std::string& Target) {
	std::set<std::string> Visiting, Done;
	auto Visit = [&](auto&& Self, const std::string& Name) -> bool {
		if (Visiting.contains(Name)) return true;
		if (Done.contains(Name)) return false;
		const auto Found = Bindings.find(Name);
		if (Found == Bindings.end()) return false;
		Visiting.insert(Name);
		for (const auto& Next : commandHeads(Found->second))
			if (Self(Self, Next)) return true;
		Visiting.erase(Name);
		Done.insert(Name);
		return false;
	};
	return Visit(Visit, Target);
}
}
