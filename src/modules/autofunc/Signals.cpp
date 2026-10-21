// Event-script signals: UserMsg hooks (WeaponList/CurWeapon/Damage/Battery/
// Health/FlashBat), the weapon-ownership diff and the `events` command.
// Split out of AutoFunc.cpp in the Phase 2 audit (old build kept the signal
// sources in core/events, the consumer in autofunc). The old add_weapon/
// drop_weapon source was the full WeaponsResource (now AbcUi's data layer);
// ownership is diffed straight from the prediction weapon_data array, names
// resolved from WeaponList messages.

#include "Signals.h"
#include "SignalValidation.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <string>

#include <metahook.h>

#include <weaponinfo.h>

#include <abc/CvarCmd.h>
#include <abc/Console.h>
#include <abc/NetMsg.h>

#include "../../services/Config.h"

namespace signals {

namespace {

constexpr int kMaxWeapons = 64; // weapon_data_t weapondata[64] (entity_state.h)

std::map<std::string, std::string> EventCmds;
std::array<std::string, kMaxWeapons> WeaponNames;
std::array<bool, kMaxWeapons> WeaponOwned = {};

// literal placeholder replace - the keys are fixed text, no regex needed
void replacePlaceholder(std::string &text, const char *key, const char *value) {
  if (!value)
    return;
  const size_t keyLength = strlen(key);
  size_t pos = 0;
  while ((pos = text.find(key, pos)) != std::string::npos) {
    text.replace(pos, keyLength, value);
    pos += strlen(value);
  }
}

#pragma region UserMsg signal hooks
#define DEFINE_SIGNAL_HOOK(name)                                               \
  static pfnUserMsgHook original_##name;                                       \
  static int __MsgFunc_##name(const char *pszName, int iSize, void *pbuf) /**/
// A message the client did not register has no predecessor; still consume
// the (absent) call instead of dereferencing null.
#define CHAIN_ORIGINAL(name)                                                   \
  (original_##name ? original_##name(pszName, iSize, pbuf) : 0)

DEFINE_SIGNAL_HOOK(WeaponList) {
  abc::MsgReader msg(pbuf, iSize);
  std::string name = msg.readString();
  msg.readChar(); // iAmmoType
  msg.readLong(); // iMax1
  msg.readChar(); // iAmmo2Type
  msg.readLong(); // iMax2
  msg.readChar(); // iSlot
  msg.readChar(); // iSlotPos
  int weaponId = msg.readShort();
  // iFlags omitted: we only need id -> name
  if (!msg.ok() || weaponId <= 0 || weaponId >= kMaxWeapons)
    return CHAIN_ORIGINAL(WeaponList);
  if (!isWeaponIdentifier(name)) {
    WeaponNames[weaponId].clear();
    abc::ConsoleWarn(
        "[AbcEssential] rejected unsafe weapon identifier for id %d\n",
        weaponId);
    return CHAIN_ORIGINAL(WeaponList);
  }
  WeaponNames[weaponId] = name;
  return CHAIN_ORIGINAL(WeaponList);
}

DEFINE_SIGNAL_HOOK(CurWeapon) {
  abc::MsgReader msg(pbuf, iSize);
  int state = msg.readByte();
  int weaponId = msg.readShort();
  msg.readLong(); // iClip
  msg.readLong(); // iClip2
  if (!msg.ok())
    return CHAIN_ORIGINAL(CurWeapon);
  if (state <= 0 && weaponId <= 0)
    triggerEvent(kEventDeath);
  return CHAIN_ORIGINAL(CurWeapon);
}

DEFINE_SIGNAL_HOOK(Damage) {
  abc::MsgReader msg(pbuf, iSize);
  int armor = msg.readByte();
  int damageTaken = msg.readByte();
  int tiles = msg.readLong();
  if (!msg.ok() || armor + damageTaken + tiles == 0)
    return CHAIN_ORIGINAL(Damage);
  msg.readCoord(); // vecFrom x/y/z, not consumed by any script
  msg.readCoord();
  msg.readCoord();
  if (!msg.ok())
    return CHAIN_ORIGINAL(Damage);
  triggerEvent(kEventDamage, std::to_string(damageTaken).c_str(),
               std::to_string(armor).c_str(), std::to_string(tiles).c_str());
  return CHAIN_ORIGINAL(Damage);
}

DEFINE_SIGNAL_HOOK(Battery) {
  abc::MsgReader msg(pbuf, iSize);
  int battery = msg.readShort();
  if (!msg.ok())
    return CHAIN_ORIGINAL(Battery);
  triggerEvent(kEventBattery, std::to_string(battery).c_str());
  return CHAIN_ORIGINAL(Battery);
}

DEFINE_SIGNAL_HOOK(Health) {
  abc::MsgReader msg(pbuf, iSize);
  int health = msg.readLong();
  if (!msg.ok())
    return CHAIN_ORIGINAL(Health);
  triggerEvent(kEventHealth, std::to_string(health).c_str());
  return CHAIN_ORIGINAL(Health);
}

DEFINE_SIGNAL_HOOK(FlashBat) {
  abc::MsgReader msg(pbuf, iSize);
  int flash = msg.readByte();
  if (!msg.ok())
    return CHAIN_ORIGINAL(FlashBat);
  triggerEvent(kEventFlashBattery, std::to_string(flash).c_str());
  return CHAIN_ORIGINAL(FlashBat);
}

// HookUserMsg returns the previous handler. A second install (a new HUD_Init
// cycle) returns our own hook; the real predecessor is already stored, so
// only a foreign predecessor is adopted. This keeps the chain correct when
// the engine rebuilt the client's usermsg table in between.
void installHook(const char *name, pfnUserMsgHook hook,
                 pfnUserMsgHook &original) {
  pfnUserMsgHook previous = g_pMetaHookAPI->HookUserMsg(name, hook);
  if (previous && previous != hook)
    original = previous;
}

void registerUserMsgHooks() {
  installHook("WeaponList", __MsgFunc_WeaponList, original_WeaponList);
  installHook("CurWeapon", __MsgFunc_CurWeapon, original_CurWeapon);
  installHook("Damage", __MsgFunc_Damage, original_Damage);
  installHook("Battery", __MsgFunc_Battery, original_Battery);
  installHook("Health", __MsgFunc_Health, original_Health);
  installHook("FlashBat", __MsgFunc_FlashBat, original_FlashBat);
}
#pragma endregion

} // namespace

void init() {
  // The events command is process-scoped (the engine keeps commands across
  // HUD_Shutdown cycles). The binding replay and the usermsg hook re-arm run
  // every cycle: shutdown() persists the bindings and the client may have
  // rebuilt its usermsg table since the last install.
  static bool Registered = false;
  if (!Registered) {
    Registered = true;
    // events command
    abc::ADD_COMMAND("events", []() {
      static auto usage = []() {
        abc::ConsoleWriteline("events [operate] [event] [cmd]\n");
        abc::ConsoleWriteline("operate: 0 = delete, 1 = set\n");
        abc::ConsoleWriteline("available events:\n");
        for (std::string_view name : kEventNames) {
          std::string line = "    [";
          line += name;
          line += "]\n";
          abc::ConsoleWriteline(line.c_str());
        }
      };
      size_t argc = gEngfuncs.Cmd_Argc();
      if (argc < 3) {
        usage();
        return;
      }
      const char *evt = gEngfuncs.Cmd_Argv(2);
      if (!evt || !isEventName(evt)) {
        usage();
        return;
      }
      const auto operation = parseOperation(gEngfuncs.Cmd_Argv(1));
      if (!operation) {
        usage();
        return;
      }
      switch (*operation) {
      case 0:
        EventCmds.erase(evt);
        break;
      case 1: {
        if (argc < 4) {
          usage();
          return;
        }
        EventCmds[evt] = gEngfuncs.Cmd_Argv(3);
        break;
      }
      default:
        usage();
        break;
      }
    });
  }
  // replay persisted event bindings
  EventCmds = abcess::config()->EventsCmds;

  registerUserMsgHooks();
}

void shutdown() {
  abcess::config()->EventsCmds = EventCmds;
}

void resetWeapons() {
  WeaponNames.fill({});
  WeaponOwned.fill(false);
}

void onTxferPredictionData(const weapon_data_s *wd) {
  for (int i = 1; i < kMaxWeapons; i++) {
    bool owned = wd[i].m_iId > 0;
    if (owned == WeaponOwned[i])
      continue;
    WeaponOwned[i] = owned;
    // scripts key on the weapon name; unknown ids (no WeaponList yet) are
    // skipped
    if (WeaponNames[i].empty())
      continue;
    triggerEvent(owned ? kEventAddWeapon : kEventDropWeapon,
                 WeaponNames[i].c_str());
  }
}

void triggerEvent(const char *eventname, const char *param1, const char *param2,
                  const char *param3, const char *param4) {
  auto found = EventCmds.find(eventname);
  if (found == EventCmds.end() || found->second.empty())
    return;
  std::string script = found->second;
  replacePlaceholder(script, "{param1}", param1);
  replacePlaceholder(script, "{param2}", param2);
  replacePlaceholder(script, "{param3}", param3);
  replacePlaceholder(script, "{param4}", param4);
  abc::EngineClientCmd(script.c_str());
}

} // namespace signals
