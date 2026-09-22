#include "platform/LegacyControlPromptBackend.h"

#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/KeyBinding.h"
#include "ps4/input/Ps4PadKeyCodes.h"

namespace
{
std::string bindingLabel(const KeyBinding *binding)
{
    if (binding == nullptr)
        return std::string();
    if (const char *name = ps4PadKeyName(binding->keyCode))
        return name;
    return GameSettings::getKeyDisplayString(binding->keyCode);
}
}

std::string legacyControlPromptLabel(const GameSettings &settings, LegacyControlAction action)
{
    switch (action)
    {
    case LegacyControlAction::Inventory: return bindingLabel(settings.keyBindInventory);
    case LegacyControlAction::Drop: return bindingLabel(settings.keyBindDrop);
    case LegacyControlAction::Jump: return bindingLabel(settings.keyBindJump);
    // Attack and use are fixed to the triggers by Ps4InputMapper.
    case LegacyControlAction::Attack: return "R2";
    case LegacyControlAction::Use: return "L2";
    }
    return std::string();
}
