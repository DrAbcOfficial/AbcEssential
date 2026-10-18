// AutoFunc: movement automation (autojump / ducktap) and the concurrent
// command chain. Ported from ABCEnchance src/core/module/autofunc.* (GBK ->
// UTF-8). Event scripts and their UserMsg signal feeds live in Signals.* -
// split out in the Phase 2 audit; the old build derived them from core/events.

#include "AutoFunc.h"
#include "CommandValidation.h"

#include <map>
#include <string>

#include <metahook.h>

#include <usercmd.h>
#include <in_buttons.h>

#include <abc/CvarCmd.h>
#include <abc/Console.h>
#include <abc/Events.h>
#include <memory>
#include <algorithm>

#include "../../services/Config.h"
#include "../Module.h"
#include "Signals.h"

namespace autofunc {

namespace {

cvar_t *AutoJumpCvar = nullptr;
bool DucktapActive = false;

struct ConcurrentCommand {
  std::string Target;
  std::string Chained;
  std::unique_ptr<abc::CChainedCommand> Hook;
  abc::CallbackList<bool()>::Handle Subscription;
};
std::map<std::string, ConcurrentCommand> ConcurrentCmds;

struct PlayerMoveState {
  bool InWater;
  bool OnGround;
  bool Walking;
};
PlayerMoveState PlayerMove = {};

void setConcurrent(const char *target, const char *chained) {
  if (!target || !chained || g_pMetaHookAPI->FindCmd(target) == nullptr)
    return;
  const std::string Key = canonicalCommand(target);
  if (Key == "concurrent" || Key == "removeconcurrent") {
    abc::ConsoleWriteline(
        "concurrent and removeconcurrent does not support this!\n");
    return;
  }
  std::map<std::string, std::string> Graph;
  for (const auto &[Name, Entry] : ConcurrentCmds)
    if (!Entry.Chained.empty())
      Graph.emplace(Name, Entry.Chained);
  Graph[Key] = chained;
  if (hasCommandCycle(Graph, Key)) {
    abc::ConsoleWriteline("There are recursive calls in concurrent command!\n");
    return;
  }
  if (!ConcurrentCmds.contains(Key) &&
      ConcurrentCmds.size() >= abc::CChainedCommand::kMaxInstances) {
    abc::ConsoleWarn("concurrent: command capacity reached\n");
    return;
  }
  auto [Found, Inserted] = ConcurrentCmds.try_emplace(Key);
  auto &Entry = Found->second;
  if (Inserted) {
    Entry.Target = target;
    Entry.Hook = std::make_unique<abc::CChainedCommand>(target);
    Entry.Subscription = Entry.Hook->Event().append([Key]() {
      const auto Found = ConcurrentCmds.find(Key);
      if (Found != ConcurrentCmds.end() && !Found->second.Chained.empty())
        abc::EngineClientCmd(Found->second.Chained.c_str());
      return true;
    });
  }
  Entry.Chained = chained;
}

void removeConcurrent(const char *target) {
  const auto Found =
      ConcurrentCmds.find(canonicalCommand(target ? target : ""));
  if (Found != ConcurrentCmds.end()) {
    // Keep the empty hook until shutdown so repeated edits do not consume
    // slots.
    Found->second.Chained.clear();
  } else {
    abc::ConsoleWriteline("There is no concurrent in this command!\n");
  }
}

} // namespace

void init() {
  AutoJumpCvar =
      abc::CREATE_CVAR("cl_autojump", "0", abc::FCVAR_VALUE, nullptr);
  abc::ADD_COMMAND("+ducktap", []() { DucktapActive = true; });
  abc::ADD_COMMAND("-ducktap", []() { DucktapActive = false; });
  // Add concurrent cmd
  abc::ADD_COMMAND("concurrent", []() {
    size_t argc = gEngfuncs.Cmd_Argc();
    if (argc < 3) {
      abc::ConsoleWriteline(
          "concurrent [target command] [concurrent command]\n");
      return;
    }
    setConcurrent(gEngfuncs.Cmd_Argv(1), gEngfuncs.Cmd_Argv(2));
  });
  abc::ADD_COMMAND("removeconcurrent", []() {
    size_t argc = gEngfuncs.Cmd_Argc();
    if (argc < 2) {
      abc::ConsoleWriteline("remove_concurrent [target command]\n");
      return;
    }
    removeConcurrent(gEngfuncs.Cmd_Argv(1));
  });
  // Replay concurrent bindings from config (base HUD commands exist by now)
  abcess::AbcEssentialConfig *config = abcess::config();
  for (auto iter = config->ConcurrentCmds.begin();
       iter != config->ConcurrentCmds.end(); iter++) {
    setConcurrent(iter->first.c_str(), iter->second.c_str());
  }
}

void shutdown() {
  abcess::AbcEssentialConfig *config = abcess::config();
  config->ConcurrentCmds.clear();
  for (auto iter = ConcurrentCmds.begin(); iter != ConcurrentCmds.end();
       iter++) {
    if (!iter->second.Chained.empty())
      config->ConcurrentCmds.emplace(iter->second.Target, iter->second.Chained);
  }
  ConcurrentCmds.clear();
  DucktapActive = false;
  PlayerMove = {};
}

// Shared air-state edge for autojump/ducktap: the button is released every
// airborne frame (a held button re-fires on landing); on the ground it is
// released when it was down the frame before (the tap).
static bool shouldReleaseButton(bool wasDown) {
  if (PlayerMove.InWater || !PlayerMove.Walking)
    return false;
  return !PlayerMove.OnGround || wasDown;
}

void autoJump(usercmd_s *cmd) {
  // Auto jump from openag
  if (AutoJumpCvar && AutoJumpCvar->value > 0) {
    static bool JumpWasDownLastFrame = false;
    if (shouldReleaseButton(JumpWasDownLastFrame))
      cmd->buttons &= ~IN_JUMP;
    JumpWasDownLastFrame = ((cmd->buttons & IN_JUMP) != 0);
  }
}

void duckTap(usercmd_s *cmd) {
  // Ducktap from bfhl
  if (DucktapActive) {
    static bool DuckWasDownLastFrame = false;
    cmd->buttons |= IN_DUCK;
    if (shouldReleaseButton(DuckWasDownLastFrame))
      cmd->buttons &= ~IN_DUCK;
    DuckWasDownLastFrame = ((cmd->buttons & IN_DUCK) != 0);
  }
}

void onPlayerMove(bool inWater, bool onGround, bool walking) {
  PlayerMove.InWater = inWater;
  PlayerMove.OnGround = onGround;
  PlayerMove.Walking = walking;
}

namespace {
class AutoFuncModule final : public abcess::IFeatureModule {
public:
  AutoFuncModule() {
    abcess::moduleRegistry().add(this);
  }
  const char *name() const override {
    return "autofunc";
  }
  void onPostInit() override {
    init();
    signals::init();
  }
  void onVidInit() override {
    signals::resetWeapons();
  }
  void onShutdown() override {
    shutdown();
    signals::shutdown();
  }
};
const AutoFuncModule RegisterModule;
} // namespace

} // namespace autofunc
