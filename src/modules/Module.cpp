#include "Module.h"

#include <abc/Console.h>

namespace abcess {

ModuleRegistry &moduleRegistry() {
  static ModuleRegistry registry;
  return registry;
}

void ModuleRegistry::add(IFeatureModule *module) {
  if (!module)
    return;
  for (size_t i = 0; i < Count; ++i)
    if (Modules[i] == module)
      return;
  if (Count >= kMaxModules) {
    abc::ConsoleWarn(
        "[AbcEssential] module registry full (%zu), '%s' not registered\n",
        kMaxModules, module->name());
    return;
  }
  Modules[Count++] = module;
}

void ModuleRegistry::initAll() {
  for (size_t i = 0; i < Count; i++)
    Modules[i]->onInit();
}

void ModuleRegistry::postInitAll() {
  for (size_t i = 0; i < Count; i++)
    Modules[i]->onPostInit();
}

void ModuleRegistry::vidInitAll() {
  for (size_t i = 0; i < Count; i++)
    Modules[i]->onVidInit();
}

void ModuleRegistry::frameAll() {
  for (size_t i = 0; i < Count; i++)
    Modules[i]->onFrame();
}

void ModuleRegistry::newMapAll() {
  for (size_t i = 0; i < Count; i++)
    Modules[i]->onNewMap();
}

void ModuleRegistry::shutdownAll() {
  // Reverse registration order: modules that initialized later may depend on
  // earlier ones, so tear them down first.
  for (size_t i = Count; i > 0; --i)
    Modules[i - 1]->onShutdown();
}

} // namespace abcess
