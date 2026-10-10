#pragma once

// Feature-module seam (Phase 2 audit): modules self-register here; the hook
// spine (src/hooks.cpp) only walks the registry at the lifecycle points
// below, it never names modules. Adding a lifecycle-only module (like find)
// is one .cpp + one CMakeLists entry, no spine edit.
//
// Hook points that are module-specific seams (V_CalcRefdef, CL_CreateMove,
// HUD_AddEntity, ...) stay as explicit calls in src/hooks.cpp - they are part
// of the hook table, not of the generic lifecycle.

#include <cstddef>

namespace abcess {

class IFeatureModule {
public:
	virtual ~IFeatureModule() = default;

	virtual const char* name() const = 0;
	// HUD_Init, before the chained base init: register cvars/commands
	// (08 §4.9 red line - never at LoadEngine).
	virtual void onInit() {}
	// HUD_Init, after the chained base init: bindings that reference
	// base-HUD commands (concurrent replay).
	virtual void onPostInit() {}
	// HUD_VidInit.
	virtual void onVidInit() {}
	// HUD_Frame, after the task-manager pump.
	virtual void onFrame() {}
	// R_NewMap: per-map state (extra precache, cached indexes).
	virtual void onNewMap() {}
	// HUD_Shutdown, before the config is saved: persist module state.
	virtual void onShutdown() {}
};

class ModuleRegistry {
public:
	static constexpr size_t kMaxModules = 16;

	// Constant-initialized (all-zero), so add() is safe from any static
	// initializer in the DLL regardless of translation-unit order.
	constexpr ModuleRegistry() = default;

	void add(IFeatureModule* module);

	void initAll();
	void postInitAll();
	void vidInitAll();
	void frameAll();
	void newMapAll();
	void shutdownAll();

private:
	IFeatureModule* Modules[kMaxModules] = {};
	size_t Count = 0;
};

// Per-DLL singleton.
ModuleRegistry& moduleRegistry();

}
